#include <fstream>
#include <ctime>
#include <chrono>

#include "Market.h"
#include "Pricer.h"
#include "RiskEngine.h"
#include "Factory.h"
#include "thread_pool.h"
#include "helper.h"


using namespace std;

struct TradeResult
{
	size_t id;
	string tradeInfo;
	double PV=0;
	double DV01=0;
	double Vega=0;
};

void loadTrade(vector<shared_ptr<Trade>>& myPortfolio)
{
	string fileName = "trade.txt";
	string header;
	vector<string> tradeData;
	readFromFile(fileName, header, tradeData);
	vector<string> tradeHeader = split(header, ";");
	for (size_t i = 0; i < tradeData.size(); i++) {
		vector<string> tradeInfo = split(tradeData[i], ";");
		int id = stoi(tradeInfo[0]);
		string type = tradeInfo[1];
		Date tradeDate = Date(tradeInfo[2]);
		Date startDate = Date(tradeInfo[3]);
		Date endDate = Date(tradeInfo[4]);
		double notional = stod(tradeInfo[5]);
		string undelrying = tradeInfo[6];
		double rate = stod(tradeInfo[7]);
		double strike = stod(tradeInfo[8]);
		double freq = stod(tradeInfo[9]);
		string optionTypeStr = tradeInfo[10];
		OptionType optionType = OptionType::None;
		if (optionTypeStr == "call")
			optionType = OptionType::Call;
		else if (optionTypeStr == "put")
			optionType = OptionType::Put;
		else
			optionType = OptionType::None;

		shared_ptr<Trade> trade;
		if (type == "bond") {
			auto bFactory = std::make_unique<BondFactory>();
			trade = bFactory->createTrade(undelrying, startDate, endDate, notional, rate, freq, optionType);
		}
		else if (type == "swap") {
			auto sFactory = std::make_unique<SwapFactory>();
			trade = sFactory->createTrade(undelrying, startDate, endDate, notional, rate, freq, optionType);
		}
		else if (type == "european") {
			auto eFactory = std::make_unique<EurOptFactory>();
			trade = eFactory->createTrade(undelrying, startDate, endDate, notional, strike, freq, optionType);
		}
		else if (type == "american") {
			auto aFactory = std::make_unique<AmericanOptFactory>();
			trade = aFactory->createTrade(undelrying, startDate, endDate, notional, strike, freq, optionType);
		}
		myPortfolio.push_back(trade);
	}
}

void loadIrCurve(Market& mkt, const string& fileName, const string& curveName)
{
	auto curve = make_shared<RateCurve>(curveName);
	string header;
	vector<string> curveData;
	readFromFile(fileName, header, curveData);
	Date valueDate = mkt.asOf; // use market date as value date for the curve
	curve->_asOf = valueDate;
	for (size_t i = 0; i < curveData.size(); i++) {
		vector<string> rateInfo = split(curveData[i], ":");
		string tenor = rateInfo[0];
		double rate = stod(rateInfo[1].substr(0, rateInfo[1].size() - 1)) / 100;
		Date tenorDate = dateAddTenor(valueDate, tenor);
		curve->addRate(tenorDate, rate);
	}
	mkt.addCurve(curveName, curve);
}

void loadVolCurve(Market& mkt, const string& fileName, const string& curveName)
{
	auto curve = make_shared<VolCurve>(curveName);
	string header;
	vector<string> curveData;
	readFromFile(fileName, header, curveData);
	Date valueDate = mkt.asOf; // use market date as value date for the curve
	curve->_asOf = valueDate;
	for (size_t i = 0; i < curveData.size(); i++) {
		vector<string> rateInfo = split(curveData[i], ":");
		string tenor = rateInfo[0];
		double vol = stod(rateInfo[1].substr(0, rateInfo[1].size() - 1)) / 100;
		Date tenorDate = dateAddTenor(valueDate, tenor);
		curve->addVol(tenorDate, vol);
	}
	mkt.addVolCurve(curveName, curve);
}

void outPutResult(const vector<TradeResult>& results)
{
	vector<string> output;
	size_t i = 0;
	for (auto re : results) {
		i++;
		string row;
		row = to_string(re.id) + "; " + re.tradeInfo + "; PV:" + to_string(re.PV) + "; Delta:" + to_string(re.DV01) + "; Vega:" + to_string(re.Vega);
		output.push_back(row);
	}
	outputToFile("output.txt", output);
}

int main()
{
	// Get the current system time
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm localTime;
	localtime_s(&localTime, &t);
	Date valueDate = Date(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);

	// step1: create market data and load curve, vol and prices into market data
	auto mkt = make_shared<Market>(valueDate);
	loadIrCurve(*mkt, "usd_curve.txt", "USD-SOFR");
	loadIrCurve(*mkt, "sgd_curve.txt", "SGD-SORA");
	loadVolCurve(*mkt, "vol.txt", "LOGVOL");
	mkt->addStockPrice("APPL", 652.0);
	mkt->addStockPrice("SP500", 5035.7);
	mkt->addStockPrice("STI", 3420);

	mkt->Print();
	auto usdCurve = mkt->getCurve("USD-SOFR");
	double rate1 = usdCurve->getRate(Date(2026, 1, 1));
	cout << "usd curve rate:" << rate1 << endl;
	auto sgdCurve = mkt->getCurve("SGD-SORA");
	double rate2 = sgdCurve->getRate(Date(2026, 4, 15));
	cout << "sgd curve rate:" << rate2 << endl;

	//step 2, create a portfolio of bond, swap, european option, american option
	vector<std::shared_ptr<Trade>> myPortfolio;
	loadTrade(myPortfolio);
	auto sFactory = std::make_unique<SwapFactory>();
	auto eFactory = std::make_unique<EurOptFactory>();
	auto swap = sFactory->createTrade("USD-SOFR", Date(2024, 1, 1), Date(2034, 1, 1), -1000000, 0.03, 1.0, OptionType::None);
	auto eCall = eFactory->createTrade("APPL", Date(2024, 1, 1), Date(2025, 1, 1), 10000, 530, 0, OptionType::Call);

	// step 3, creat a pricer and price the portfolio, output the pricing result of each deal 
	vector<TradeResult> results;
	auto pricer = make_shared<CRRBinomialTreePricer>(50);
	for (size_t i = 0; i < myPortfolio.size(); i++) {
		auto& trade = myPortfolio[i];
		double pv = pricer->Price(*mkt, trade);
		//log pv details out in a file
		TradeResult re;
		re.id = i + 1;
		re.tradeInfo = trade->getType() + " " + trade->getUnderlying();
		re.PV = pv;
		results.push_back(re);
	}

	//task 4, compute the Greeks of DV01, and Vega risk as of market date 1
	// 4.1 compute risk using risk engine
	// 4.2 use idea of multi-threading
	// analyzing the pv and risk

	// sample code for risk computation
	double curve_shock = 0.0001;// 1 bp of zero rate
	double vol_shock = 0.01; //1% of log normal vol
	double price_shock = 1.0; // shock in abs price of stock

	// example 1, simple example of computing one point dv01 for one swap
	string risk_id = "USD-SOFR:DV01:DEAL 01";
	double shockUp = 0.0001;
	//double shockDown = -0.0001;
	auto testShockUp = MarketShock();
	testShockUp.market_id = "USD-SOFR";
	testShockUp.shock = make_pair(Date(), shockUp);
	//auto testShockDown = MarketShock();
	//testShockDown.market_id = "USD-SOFR";
	//testShockDown.shock = make_pair(Date(), shockDown);
	auto shockedUpCurveUp = CurveDecorator(*mkt, testShockUp);
	//auto shockedUpCurveDown = CurveDecorator(*mkt, testShockDown);

	unordered_map<string, double> thisDealDv01;
	double pv_up, pv_down;
	auto m_up = shockedUpCurveUp.getMarketUp();
	pv_up = swap->Pv(m_up);
	auto m_down = shockedUpCurveUp.getMarketDown();
	pv_down = swap->Pv(m_down);
	double dv01 = (pv_up - pv_down) / 2.0;
	thisDealDv01.emplace(risk_id, dv01);

	//example2, using risk engine to compute full set of dv01 for a swap
	RiskEngine re(*mkt, curve_shock, vol_shock, price_shock);
	re.computeRisk("dv01", swap, true);
	auto dv01_of_swap = re.getResult();

	//example 3, demo using thread pool
	if (true) {
		map<string, double> swapDv01;
		ThreadPool pool(4);

		auto pv_job = [&swapDv01, risk_id, &eCall, &m_up, &m_down]() {
			cout << "Task is running on thread: " << this_thread::get_id() << endl;
			auto pricer = std::make_unique<CRRBinomialTreePricer>(50);
			double pv_u = pricer->Price(m_up, eCall);
			double pv_d = pricer->Price(m_down, eCall);
			double dv01 = (pv_u - pv_d) / 2.;
			swapDv01.emplace(std::make_pair(risk_id, dv01));
			this_thread::sleep_for(chrono::milliseconds(100));
			};

		for (int i = 0; i < 5; ++i) {
			pool.enqueue(pv_job);
		}
	}


	// step 5, output result to file
	outPutResult(results);

	//final
	cout << "Project build successfully!" << endl;

	return 0;

}
