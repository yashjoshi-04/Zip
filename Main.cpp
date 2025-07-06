#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip> // For std::fixed and std::setprecision
#include <algorithm> // For std::transform for to_lower
#include <cmath>     // For std::abs, std::max for BS
#include <chrono>    // For system_clock
#include <ctime>     // For time_t, tm
#include <map>       // For map in TradeResult

#include "Market.h"
#include "Pricer.h"
#include "RiskEngine.h"
#include "Factory.h"
#include "helper.h"
#include "Types.h"
#include "TreeProduct.h"
#include "EuropeanTrade.h"
#include "AmericanTrade.h"

// Forward declaration for BlackScholes pricing function (defined in Pricer.cpp)
namespace BlackScholes {
    double PriceEuropeanOption(double S, double K, double T_years, double r, double sigma, OptionType type);
}

using namespace std;

// Helper trim function
std::string trim(const std::string& str_to_trim) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str_to_trim.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = str_to_trim.find_last_not_of(whitespace);
    return str_to_trim.substr(start, end - start + 1);
}

struct TradeResult
{
	size_t id_num;
	string tradeInfo;
	double PV=0.0;
	map<string, double> dv01_map;
    double total_dv01 = 0.0;
	map<string, double> vega_map;
    double total_vega = 0.0;
    double blackScholesPv = 0.0;
    bool isEuropeanOption = false;
};

void loadTrade(vector<shared_ptr<Trade>>& myPortfolio)
{
	string fileName = "trade.txt";
	string header;
	vector<string> tradeData_lines;
	readFromFile(fileName, header, tradeData_lines);

	for (size_t idx = 0; idx < tradeData_lines.size(); ++idx) {
		vector<string> tradeInfo_row = split(tradeData_lines[idx], ";");
		if (tradeInfo_row.size() < 12) {
            cerr << "Warning: Skipping malformed trade line (idx " << idx << ", expected 12+ cols, got " << tradeInfo_row.size() << "): " << tradeData_lines[idx] << endl;
            continue;
        }

		// string id_str = trim(tradeInfo_row[0]); // Unused variable 'id' - commented out
		string type_str = trim(tradeInfo_row[1]);
        std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

		// Date tradeDate_val = Date(trim(tradeInfo_row[2])); // Unused variable 'tradeDate' - commented out
		Date startDate_val = Date(trim(tradeInfo_row[3]));
		Date endDate_val = Date(trim(tradeInfo_row[4]));
		double notional_val = stod(trim(tradeInfo_row[5]));
		string underlying_str = trim(tradeInfo_row[6]);
		double rate_param = stod(trim(tradeInfo_row[7]));
		double strike_param = stod(trim(tradeInfo_row[8]));
		double freq_param = stod(trim(tradeInfo_row[9]));
		string optionTypeStr_val = trim(tradeInfo_row[10]);
        std::transform(optionTypeStr_val.begin(), optionTypeStr_val.end(), optionTypeStr_val.begin(), ::tolower);
        string directionStr_val = trim(tradeInfo_row[11]);
        std::transform(directionStr_val.begin(), directionStr_val.end(), directionStr_val.begin(), ::tolower);

		OptionType optionType_val = OptionType::None;
		if (optionTypeStr_val == "call") optionType_val = OptionType::Call;
		else if (optionTypeStr_val == "put") optionType_val = OptionType::Put;
        else if (optionTypeStr_val == "binarycall") optionType_val = OptionType::BinaryCall;
        else if (optionTypeStr_val == "binaryput") optionType_val = OptionType::BinaryPut;

        if (type_str == "swap") {
            if (directionStr_val == "pay") notional_val = -std::abs(notional_val);
            else if (directionStr_val == "receive") notional_val = std::abs(notional_val);
            else { notional_val = std::abs(notional_val); }
        } else if (type_str == "bond" || type_str == "european" || type_str == "american") {
            if (directionStr_val == "short") notional_val = -std::abs(notional_val);
            else if (directionStr_val == "long") notional_val = std::abs(notional_val);
            else { notional_val = std::abs(notional_val); }
        }

		shared_ptr<Trade> trade_ptr;
		if (type_str == "bond") {
			auto bFactory = std::make_unique<BondFactory>();
			trade_ptr = bFactory->createTrade(underlying_str, startDate_val, endDate_val, notional_val, rate_param, freq_param, optionType_val);
		}
		else if (type_str == "swap") {
			auto sFactory = std::make_unique<SwapFactory>();
			trade_ptr = sFactory->createTrade(underlying_str, startDate_val, endDate_val, notional_val, rate_param, freq_param, optionType_val);
		}
		else if (type_str == "european") {
			auto eFactory = std::make_unique<EurOptFactory>();
			trade_ptr = eFactory->createTrade(underlying_str, startDate_val, endDate_val, notional_val, strike_param, 0, optionType_val);
		}
		else if (type_str == "american") {
			auto aFactory = std::make_unique<AmericanOptFactory>();
			trade_ptr = aFactory->createTrade(underlying_str, startDate_val, endDate_val, notional_val, strike_param, 0, optionType_val);
		} else {
            cerr << "Warning (Trade " << idx+1 << "): Unknown trade type in file: " << type_str << ". Skipping trade." << endl;
            continue;
        }
		myPortfolio.push_back(trade_ptr);
	}
}

void loadIrCurve(Market& mkt, const string& fileName, const string& curveName_arg)
{
	auto curve = make_shared<RateCurve>(curveName_arg);
	string file_header;
	vector<string> curveData_lines;
	readFromFile(fileName, file_header, curveData_lines);
	Date valueDate_market = mkt.asOf;
	curve->_asOf = valueDate_market;
	for (const auto& line : curveData_lines) {
		vector<string> rateInfo_parts = split(line, ":");
        if (rateInfo_parts.size() != 2) {
            continue;
        }
		string tenor_str = trim(rateInfo_parts[0]);
        string rate_str_val = trim(rateInfo_parts[1]);
        if (!rate_str_val.empty() && rate_str_val.back() == '%') {
            rate_str_val.pop_back();
        }
		double rate_val = stod(rate_str_val) / 100.0;
		Date tenorDate_val = dateAddTenor(valueDate_market, tenor_str);
		curve->addRate(tenorDate_val, rate_val);
	}
	mkt.addCurve(curveName_arg, curve);
}

void loadVolCurve(Market& mkt, const string& fileName, const string& curveName_arg)
{
	auto curve = make_shared<VolCurve>(curveName_arg);
	string file_header;
	vector<string> curveData_lines;
	readFromFile(fileName, file_header, curveData_lines);
	Date valueDate_market = mkt.asOf;
	curve->_asOf = valueDate_market;
	for (const auto& line : curveData_lines) {
		vector<string> volInfo_parts = split(line, ":");
         if (volInfo_parts.size() != 2) {
            continue;
        }
		string tenor_str = trim(volInfo_parts[0]);
		string vol_str_val = trim(volInfo_parts[1]);
        if (!vol_str_val.empty() && vol_str_val.back() == '%') {
            vol_str_val.pop_back();
        }
		double vol_val = stod(vol_str_val) / 100.0;
		Date tenorDate_val = dateAddTenor(valueDate_market, tenor_str);
		curve->addVol(tenorDate_val, vol_val);
	}
	mkt.addVolCurve(curveName_arg, curve);
}

void loadStockPrices(Market& mkt, const std::string& fileName) {
    std::string header_dummy;
    std::vector<std::string> priceData_lines;
    try {
        readFromFile(fileName, header_dummy, priceData_lines);
    } catch (const std::exception& e) {
        std::cerr << "Error reading stock price file '" << fileName << "': " << e.what() << std::endl;
        return;
    }
    for (const std::string& line : priceData_lines) {
        std::vector<std::string> stockInfo_parts = split(line, ":");
        if (stockInfo_parts.size() == 2) {
            std::string stockName_val = trim(stockInfo_parts[0]);
            double price_val = std::stod(trim(stockInfo_parts[1]));
            mkt.addStockPrice(stockName_val, price_val);
        }
    }
}

void outputResultToFile(const vector<TradeResult>& results_vec, const string& filename)
{
	vector<string> output_str_lines;
    output_str_lines.push_back("TradeID;TradeInfo;PV;Total_DV01;Total_Vega;BS_PV(EurOptOnly)");
	for (const auto& res_item : results_vec) {
		string row_str;
		row_str = to_string(res_item.id_num) + ";" +
              res_item.tradeInfo + ";" +
              "PV:" + to_string(res_item.PV) + ";" +
              "Total_DV01:" + to_string(res_item.total_dv01) + ";" +
              "Total_Vega:" + to_string(res_item.total_vega);
        if (res_item.isEuropeanOption) {
            row_str += ";BS_PV:" + to_string(res_item.blackScholesPv);
        } else {
            row_str += ";BS_PV:NA";
        }
		output_str_lines.push_back(row_str);
	}
	outputToFile(filename, output_str_lines);
    cout << "Results written to " << filename << endl;
}

void generateWriteUp(const vector<TradeResult>& results_vec, const Market& mkt_context) {
    vector<string> writeup_lines_vec;
    writeup_lines_vec.push_back("# Project Write-Up");
    writeup_lines_vec.push_back("## Observations and Explanations");
    writeup_lines_vec.push_back("");
    writeup_lines_vec.push_back("### a. Comparing Tree Model Price of European vs Black-Scholes");
    bool foundEurOpt = false;
    for(const auto& res_item : results_vec) {
        if (res_item.isEuropeanOption) {
            foundEurOpt = true;
            writeup_lines_vec.push_back("Trade ID " + to_string(res_item.id_num) + " (" + res_item.tradeInfo + "):");
            writeup_lines_vec.push_back("  - Binomial Tree PV (N=50): " + to_string(res_item.PV));
            writeup_lines_vec.push_back("  - Black-Scholes PV:      " + to_string(res_item.blackScholesPv));
            double diff_val = res_item.PV - res_item.blackScholesPv;
            double rel_diff_pct = 0.0;
            if (std::abs(res_item.blackScholesPv) > 1e-9) {
                 rel_diff_pct = (diff_val / res_item.blackScholesPv) * 100.0;
            } else if (std::abs(diff_val) < 1e-9) {
                rel_diff_pct = 0.0;
            } else {
                rel_diff_pct = (diff_val > 0) ? 1.0e6 : -1.0e6;
            }
            writeup_lines_vec.push_back("  - Difference (Tree - BS): " + to_string(diff_val) + " (" + to_string(rel_diff_pct) + "%)");
        }
    }
    if (!foundEurOpt) {
        writeup_lines_vec.push_back("No European options found in the portfolio for this comparison.");
    }
    writeup_lines_vec.push_back("\nExplanation of differences (European Tree vs BS):");
    writeup_lines_vec.push_back("- The Black-Scholes (BS) model is an analytical solution under specific assumptions (e.g., constant volatility, continuous trading).");
    writeup_lines_vec.push_back("- The Binomial Tree is a numerical approximation that discretizes time and price movements. It converges to BS as time steps (N) increase.");
    writeup_lines_vec.push_back("- For N=50, some discretization error is expected. Differences can also arise from how tree parameters (u, d, p) are set to match the continuous model's moments.\n");

    writeup_lines_vec.push_back("### b. Comparing American and European Trade PV");
    writeup_lines_vec.push_back("To compare American vs. European PVs, one would look for pairs of options with identical parameters (underlying, strike, expiry, type) but differing exercise styles.");
    writeup_lines_vec.push_back("Example: Trade 8 (European APPL Call K=625) vs. Trade 12 (American APPL Call K=625) from trade.txt if parameters match perfectly.\n");
    writeup_lines_vec.push_back("General Explanation:");
    writeup_lines_vec.push_back("- American options can be exercised at any time up to expiry, while European options only at expiry.");
    writeup_lines_vec.push_back("- This early exercise right can have value, making American options potentially more expensive. This extra value is the 'early exercise premium'.");
    writeup_lines_vec.push_back("- For American Puts, early exercise can be optimal (e.g., deep in-the-money puts, or before dividends on the underlying if it were a call).");
    writeup_lines_vec.push_back("- For American Calls on non-dividend paying stocks (assumed here for simplicity), early exercise is generally not optimal. Thus, their PV should be very close to their European counterparts.");
    writeup_lines_vec.push_back("- If the underlying pays dividends, American Calls might be exercised early (just before an ex-dividend date) making them more valuable than European Calls.");
    writeup_lines_vec.push_back("- The Binomial Tree model captures this by comparing exercise value vs. continuation value at each node for American options.\n");

    outputToFile("write_up.txt", writeup_lines_vec);
    cout << "Write-up generated: write_up.txt" << endl;
}

int main()
{
    cout << std::fixed << std::setprecision(6);

	auto now_chrono_val = std::chrono::system_clock::now();
	std::time_t t_now_val = std::chrono::system_clock::to_time_t(now_chrono_val);
	std::tm localTime_val; // Corrected variable name
	#ifdef _WIN32
		localtime_s(&localTime_val, &t_now_val); // Windows specific
	#else
		localtime_r(&t_now_val, &localTime_val); // POSIX compliant (macOS, Linux)
	#endif
	Date valueDate_main = Date(localTime_val.tm_year + 1900, localTime_val.tm_mon + 1, localTime_val.tm_mday);
	cout << "Valuation Date: " << valueDate_main << endl << endl;

	auto mkt_ptr = make_shared<Market>(valueDate_main);
	loadIrCurve(*mkt_ptr, "usd_curve.txt", "USD-SOFR");
	loadIrCurve(*mkt_ptr, "sgd_curve.txt", "SGD-SORA");
	loadVolCurve(*mkt_ptr, "vol.txt", "LOGVOL");
    loadStockPrices(*mkt_ptr, "stockPrice.txt");

	vector<std::shared_ptr<Trade>> myPortfolio_vec;
	loadTrade(myPortfolio_vec);
    cout << "Portfolio loaded with " << myPortfolio_vec.size() << " trades." << endl << endl;

	vector<TradeResult> results_vec;
	auto pricer_ptr = make_shared<CRRBinomialTreePricer>(50);

	cout << "Pricing portfolio and calculating Black-Scholes for European options..." << endl;
	for (size_t i = 0; i < myPortfolio_vec.size(); ++i) {
		const auto& trade_item = myPortfolio_vec[i];
		TradeResult res_item;
		res_item.id_num = i + 1;

        string trade_type_info_str = trade_item->getType();
        if (auto tree_prod_ptr = dynamic_cast<TreeProduct*>(trade_item.get())) {
            trade_type_info_str += (tree_prod_ptr->getOptType() == OptionType::Call ? " Call K=" : (tree_prod_ptr->getOptType() == OptionType::Put ? " Put K=" : " Opt K=")) + to_string(tree_prod_ptr->getStrike());
            if (dynamic_cast<EuropeanOption*>(trade_item.get())) {
                res_item.isEuropeanOption = true;
            }
        }
        res_item.tradeInfo = trade_type_info_str + " " + trade_item->getUnderlying();

		res_item.PV = pricer_ptr->Price(*mkt_ptr, trade_item);

        if (res_item.isEuropeanOption) {
            auto euroOpt_ptr = static_cast<EuropeanOption*>(trade_item.get());
            double T_days = (euroOpt_ptr->GetExpiry() - mkt_ptr->asOf);

            if (T_days > 1e-6) {
                double T_years = T_days / 365.0;
                double S0_val = mkt_ptr->getStockPrice(euroOpt_ptr->getUnderlying());
                std::shared_ptr<VolCurve> vc_ptr = mkt_ptr->getVolCurve("LOGVOL");
                if (!vc_ptr) throw runtime_error("LOGVOL curve not found for BS pricing.");
                double sigma_val = vc_ptr->getVol(euroOpt_ptr->GetExpiry());

                std::shared_ptr<RateCurve> rc_opt_ptr = mkt_ptr->getCurve(euroOpt_ptr->getRateCurve());
                if (!rc_opt_ptr) throw runtime_error("Rate curve " + euroOpt_ptr->getRateCurve() + " not found for BS pricing.");
                double r_val = rc_opt_ptr->getRate(euroOpt_ptr->GetExpiry());

                res_item.blackScholesPv = BlackScholes::PriceEuropeanOption(S0_val, euroOpt_ptr->getStrike(), T_years, r_val, sigma_val, euroOpt_ptr->getOptType()) * euroOpt_ptr->getNotional();
            } else {
                 double S0_val = mkt_ptr->getStockPrice(euroOpt_ptr->getUnderlying());
                 res_item.blackScholesPv = euroOpt_ptr->Payoff(S0_val) * euroOpt_ptr->getNotional();
            }
        }
		results_vec.push_back(res_item);
	}
    cout << "Portfolio pricing complete." << endl << endl;

    cout << "Calculating Greeks (DV01, Vega)..." << endl;
	double curve_shock_val_main = 0.0001;
	double vol_shock_val_main = 0.01;
	double price_shock_val_main = 1.0;
	RiskEngine risk_engine_main(*mkt_ptr, curve_shock_val_main, vol_shock_val_main, price_shock_val_main);

    bool use_multithreading_for_risk_main = false;

	for (size_t i = 0; i < myPortfolio_vec.size(); ++i) {
		auto& trade_item = myPortfolio_vec[i];

		risk_engine_main.computeRisk(trade_item, "DV01", pricer_ptr, !use_multithreading_for_risk_main);
		results_vec[i].dv01_map = risk_engine_main.getResult();
        for(const auto& pair_item : results_vec[i].dv01_map) results_vec[i].total_dv01 += pair_item.second;

        if (dynamic_cast<TreeProduct*>(trade_item.get())) {
            risk_engine_main.computeRisk(trade_item, "VEGA", pricer_ptr, !use_multithreading_for_risk_main);
            results_vec[i].vega_map = risk_engine_main.getResult();
            for(const auto& pair_item : results_vec[i].vega_map) results_vec[i].total_vega += pair_item.second;
        } else {
            results_vec[i].total_vega = 0.0;
        }
	}
    cout << "Greek calculations complete." << endl << endl;

	outputResultToFile(results_vec, "output_pv_greeks.txt");
    generateWriteUp(results_vec, *mkt_ptr);

	cout << endl << "Project tasks completed successfully!" << endl;
	return 0;
}

```

**2. Updating `Pricer.h`**
