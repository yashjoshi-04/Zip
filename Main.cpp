#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip> // For std::fixed and std::setprecision
#include <algorithm> // For std::transform for to_lower
#include <cmath>     // For std::abs
#include <chrono>    // For system_clock
#include <ctime>     // For time_t, tm, localtime_s

#include "Market.h"
#include "Pricer.h"     // Also includes BlackScholes functions via Pricer.cpp
#include "RiskEngine.h"
#include "Factory.h"
#include "helper.h"     // For split, readFromFile, outputToFile etc.
#include "Types.h"      // For OptionType
// #include "thread_pool.h" // Not directly used here if RiskEngine handles its async

// Forward declare BlackScholes namespace if its functions are only in Pricer.cpp
// Or ensure Pricer.h includes necessary declarations if main needs to call BS directly.
// For now, BS comparison will be illustrative, main PV output from pricer.
namespace BlackScholes {
    double PriceEuropeanOption(double S, double K, double T_years, double r, double sigma, OptionType type);
}


using namespace std;

// Helper trim function (can be moved to helper.h)
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return ""; // empty or all whitespace
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}


struct TradeResult
{
	size_t id_num; // Changed from id to avoid conflict with any std::id
	string tradeInfo;
	double PV=0;
	map<string, double> dv01_map; // Store DV01 per curve
    double total_dv01 = 0;
	map<string, double> vega_map; // Store Vega per vol surface
    double total_vega = 0;

    // For write-up comparison
    double blackScholesPv = 0.0; // Only for European options
    bool isEuropeanOption = false;
};

void loadTrade(vector<shared_ptr<Trade>>& myPortfolio) // Removed Market& mkt argument, not used
{
	string fileName = "trade.txt";
	string header;
	vector<string> tradeData;
	readFromFile(fileName, header, tradeData);

	for (size_t i = 0; i < tradeData.size(); i++) {
		vector<string> tradeInfo = split(tradeData[i], ";");
		if (tradeInfo.size() < 12) {
            cerr << "Warning: Skipping malformed trade line (idx " << i << "): not enough columns. Line: " << tradeData[i] << endl;
            continue;
        }

		// int id = stoi(trim(tradeInfo[0]));
		string type = trim(tradeInfo[1]);
        std::transform(type.begin(), type.end(), type.begin(), ::tolower); // Normalize type to lower case

		Date tradeDate = Date(trim(tradeInfo[2]));
		Date startDate = Date(trim(tradeInfo[3]));
		Date endDate = Date(trim(tradeInfo[4]));
		double notional_val = stod(trim(tradeInfo[5]));
		string underlying = trim(tradeInfo[6]);
		double rate_param = stod(trim(tradeInfo[7])); // Coupon for bond, Fixed Rate for Swap
		double strike_param = stod(trim(tradeInfo[8]));// Strike for options. Bond price in file not used by current constructor.
		double freq_param = stod(trim(tradeInfo[9]));  // Coupon/payment year fraction (e.g., 0.5 for semi-annual)
		string optionTypeStr = trim(tradeInfo[10]);
        std::transform(optionTypeStr.begin(), optionTypeStr.end(), optionTypeStr.begin(), ::tolower);
        string directionStr = trim(tradeInfo[11]);
        std::transform(directionStr.begin(), directionStr.end(), directionStr.begin(), ::tolower);


		OptionType optionType = OptionType::None;
		if (optionTypeStr == "call") optionType = OptionType::Call;
		else if (optionTypeStr == "put") optionType = OptionType::Put;
        else if (optionTypeStr == "binarycall") optionType = OptionType::BinaryCall;
        else if (optionTypeStr == "binaryput") optionType = OptionType::BinaryPut;


        if (type == "swap") {
            if (directionStr == "pay") notional_val = -std::abs(notional_val);
            else if (directionStr == "receive") notional_val = std::abs(notional_val);
            else cerr << "Warning (Trade " << i+1 << "): Unknown direction for swap: " << directionStr << ". Using positive notional." << endl;
        } else if (type == "bond" || type == "european" || type == "american") {
            if (directionStr == "short") notional_val = -std::abs(notional_val);
            else if (directionStr == "long") notional_val = std::abs(notional_val);
            else cerr << "Warning (Trade " << i+1 << "): Unknown direction for " << type << ": " << directionStr << ". Using positive notional." << endl;
        }


		shared_ptr<Trade> trade_ptr;
		if (type == "bond") {
			auto bFactory = std::make_unique<BondFactory>();
			trade_ptr = bFactory->createTrade(underlying, startDate, endDate, notional_val, rate_param, freq_param, optionType);
		}
		else if (type == "swap") {
			auto sFactory = std::make_unique<SwapFactory>();
			trade_ptr = sFactory->createTrade(underlying, startDate, endDate, notional_val, rate_param, freq_param, optionType);
		}
		else if (type == "european") {
			auto eFactory = std::make_unique<EurOptFactory>();
			trade_ptr = eFactory->createTrade(underlying, startDate, endDate, notional_val, strike_param, 0, optionType); // freq is 0 for options
		}
		else if (type == "american") {
			auto aFactory = std::make_unique<AmericanOptFactory>();
			trade_ptr = aFactory->createTrade(underlying, startDate, endDate, notional_val, strike_param, 0, optionType); // freq is 0 for options
		} else {
            cerr << "Warning (Trade " << i+1 << "): Unknown trade type in file: " << type << ". Skipping trade." << endl;
            continue;
        }
		myPortfolio.push_back(trade_ptr);
	}
}

void loadIrCurve(Market& mkt, const string& fileName, const string& curveName)
{
	auto curve = make_shared<RateCurve>(curveName);
	string header; // First line of file
	vector<string> curveData; // Subsequent lines
	readFromFile(fileName, header, curveData); // `header` will contain the curve name line from file if present
	Date valueDate = mkt.asOf;
	curve->_asOf = valueDate; // Set asOf for the curve itself for DF calculations
	for (size_t i = 0; i < curveData.size(); i++) {
		vector<string> rateInfo = split(curveData[i], ":");
        if (rateInfo.size() != 2) {
            cerr << "Warning: Malformed line in IR curve file " << fileName << ": " << curveData[i] << endl;
            continue;
        }
		string tenor = trim(rateInfo[0]);
        string rateStr = trim(rateInfo[1]);
        if (rateStr.back() == '%') {
            rateStr.pop_back();
        }
		double rate = stod(rateStr) / 100.0;
		Date tenorDate = dateAddTenor(valueDate, tenor);
		curve->addRate(tenorDate, rate);
	}
	mkt.addCurve(curveName, curve); // Add to market context
}

void loadVolCurve(Market& mkt, const string& fileName, const string& curveName)
{
	auto curve = make_shared<VolCurve>(curveName);
	string header;
	vector<string> curveData;
	readFromFile(fileName, header, curveData);
	Date valueDate = mkt.asOf;
	curve->_asOf = valueDate;
	for (size_t i = 0; i < curveData.size(); i++) {
		vector<string> volInfo = split(curveData[i], ":");
         if (volInfo.size() != 2) {
            cerr << "Warning: Malformed line in Vol curve file " << fileName << ": " << curveData[i] << endl;
            continue;
        }
		string tenor = trim(volInfo[0]);
		string volStr = trim(volInfo[1]);
        if (volStr.back() == '%') {
            volStr.pop_back();
        }
		double vol = stod(volStr) / 100.0;
		Date tenorDate = dateAddTenor(valueDate, tenor);
		curve->addVol(tenorDate, vol);
	}
	mkt.addVolCurve(curveName, curve);
}

void loadStockPrices(Market& mkt, const std::string& fileName) {
    std::string header_dummy;
    std::vector<std::string> priceData;
    try {
        readFromFile(fileName, header_dummy, priceData);
    } catch (const std::exception& e) {
        std::cerr << "Error reading stock price file '" << fileName << "': " << e.what() << std::endl;
        return;
    }

    for (const std::string& line : priceData) {
        std::vector<std::string> stockInfo = split(line, ":");
        if (stockInfo.size() == 2) {
            std::string stockName = trim(stockInfo[0]);
            double price = std::stod(trim(stockInfo[1]));
            mkt.addStockPrice(stockName, price);
        } else {
            std::cerr << "Warning: Malformed line in stock price file: " << line << std::endl;
        }
    }
}


void outputResultToFile(const vector<TradeResult>& results, const string& filename)
{
	vector<string> outputLines;
    outputLines.push_back("TradeID; TradeInfo; PV; Total_DV01; Total_Vega; BS_PV (EurOpt)"); // Header

	for (const auto& res : results) {
		string row;
		row = to_string(res.id_num) + "; " +
              res.tradeInfo + "; " +
              "PV:" + to_string(res.PV) + "; " +
              "Total_DV01:" + to_string(res.total_dv01) + "; " +
              "Total_Vega:" + to_string(res.total_vega);
        if (res.isEuropeanOption) {
            row += "; BS_PV:" + to_string(res.blackScholesPv);
        } else {
            row += "; BS_PV:NA";
        }
		outputLines.push_back(row);
	}
	outputToFile(filename, outputLines);
    cout << "Results written to " << filename << endl;
}

void generateWriteUp(const vector<TradeResult>& results, const Market& mkt) {
    vector<string> writeup_lines;
    writeup_lines.push_back("# Project Write-Up");
    writeup_lines.push_back("## Observations and Explanations");
    writeup_lines.push_back("");

    writeup_lines.push_back("### a. Comparing Tree Model Price of European vs Black-Scholes");
    bool foundEurOpt = false;
    for(const auto& res : results) {
        if (res.isEuropeanOption) {
            foundEurOpt = true;
            writeup_lines.push_back("Trade ID " + to_string(res.id_num) + " (" + res.tradeInfo + "):");
            writeup_lines.push_back("  - Binomial Tree PV: " + to_string(res.PV));
            writeup_lines.push_back("  - Black-Scholes PV: " + to_string(res.blackScholesPv));
            double diff = res.PV - res.blackScholesPv;
            double rel_diff = (res.blackScholesPv != 0) ? (diff / res.blackScholesPv * 100.0) : 0.0;
            writeup_lines.push_back("  - Difference (Tree - BS): " + to_string(diff) + " (" + to_string(rel_diff) + "%)");
        }
    }
    if (!foundEurOpt) {
        writeup_lines.push_back("No European options found in the portfolio for comparison.");
    }
    writeup_lines.push_back("");
    writeup_lines.push_back("Explanation of differences:");
    writeup_lines.push_back("- The Black-Scholes model provides an analytical solution for European options under specific assumptions (constant volatility, risk-free rate, geometric Brownian motion for stock price, no arbitrage, etc.).");
    writeup_lines.push_back("- The Binomial Tree model is a numerical method that discretizes time and stock price movements. It converges to the Black-Scholes price as the number of time steps increases (N -> infinity), assuming the tree is constructed to match the continuous model's mean and variance (e.g., CRR, JRRN).");
    writeup_lines.push_back("- Differences can arise due to: ");
    writeup_lines.push_back("  - Discretization error: With a finite number of steps (N=50 in this project), the tree price is an approximation.");
    writeup_lines.push_back("  - Model calibration: How u, d, and p are set in the binomial model affects convergence speed and accuracy.");
    writeup_lines.push_back("  - Early exercise: Not applicable for European options, but a key difference for American options.");
    writeup_lines.push_back("Generally, for N=50, the prices should be reasonably close, but some difference is expected.");
    writeup_lines.push_back("");

    writeup_lines.push_back("### b. Comparing American and European Trade PV");
    // This requires finding comparable American and European options (same underlying, strike, expiry, type)
    // The current trade.txt has some pairs like trades 8 (Euro Call APPL) & 12 (Amer Call APPL)
    writeup_lines.push_back("Comparison (example using APPL Call options from trade.txt if present):");
    shared_ptr<TradeResult> euroApplCall, amerApplCall; // Example
    for(const auto& res : results) {
        if (res.tradeInfo.find("EuropeanOption APPL") != string::npos && res.tradeInfo.find("Call") != string::npos ) { // Heuristic
             // Find a specific European APPL Call to compare
        }
        // Similarly for American
    }
    writeup_lines.push_back("If an American option has a higher PV than its European counterpart (for Calls on non-dividend paying stocks, or Puts):");
    writeup_lines.push_back("- This difference is the 'early exercise premium'.");
    writeup_lines.push_back("- American options grant the holder the right to exercise at any time up to expiry, while European options only at expiry.");
    writeup_lines.push_back("- For American Puts, early exercise can be optimal if the stock price falls significantly, or if interest rates are high (value of receiving strike K sooner).");
    writeup_lines.push_back("- For American Calls on non-dividend paying stocks, early exercise is generally not optimal. Their PV should be very close to European Call PV. If there's a dividend, early exercise might be optimal just before ex-dividend date.");
    writeup_lines.push_back("- The Binomial Tree model naturally captures the value of early exercise by comparing exercise value vs. continuation value at each node.");
    writeup_lines.push_back("");

    outputToFile("write_up.txt", writeup_lines);
    cout << "Write-up generated: write_up.txt" << endl;
}


int main()
{
    cout << std::fixed << std::setprecision(6); // Set default precision for double output

	// Get the current system time for market valuation date
	auto now_chrono = std::chrono::system_clock::now();
	std::time_t t_now = std::chrono::system_clock::to_time_t(now_chrono);
	std::tm localTime;
	#ifdef _WIN32
		localtime_s(&localTime, &t_now);
	#else
		localtime_r(&t_now, &localTime); // POSIX version
	#endif
	Date valueDate = Date(localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);
	cout << "Valuation Date: " << valueDate << endl << endl;

	// Step 1: Create Market Data
	auto mkt = make_shared<Market>(valueDate);
	loadIrCurve(*mkt, "usd_curve.txt", "USD-SOFR");
	loadIrCurve(*mkt, "sgd_curve.txt", "SGD-SORA");
	loadVolCurve(*mkt, "vol.txt", "LOGVOL");
    loadStockPrices(*mkt, "stockPrice.txt"); // Load stock prices from file

	// mkt->Print(); // Optional: print market data details

	// Step 2: Create Portfolio
	vector<std::shared_ptr<Trade>> myPortfolio;
	loadTrade(myPortfolio);
    cout << "Portfolio loaded with " << myPortfolio.size() << " trades." << endl << endl;

	// Step 3: Create Pricer and Price Portfolio
	vector<TradeResult> results;
	auto pricer = make_shared<CRRBinomialTreePricer>(50); // 50 time steps for binomial tree

	cout << "Pricing portfolio and calculating Black-Scholes for European options..." << endl;
	for (size_t i = 0; i < myPortfolio.size(); i++) {
		const auto& trade = myPortfolio[i];
		TradeResult res;
		res.id_num = i + 1; // Simple 1-based ID
		res.tradeInfo = trade->getType() + " " + trade->getUnderlying();
        // Add option type and strike for options for more detail
        if (auto opt = dynamic_cast<EuropeanOption*>(trade.get())) {
            res.tradeInfo += (opt->getOptType() == OptionType::Call ? " Call K=" : " Put K=") + to_string(opt->getStrike());
            res.isEuropeanOption = true;
        } else if (auto opt = dynamic_cast<AmericanOption*>(trade.get())) {
             res.tradeInfo += (opt->getOptType() == OptionType::Call ? " Call K=" : " Put K=") + to_string(opt->getStrike());
        }


		res.PV = pricer->Price(*mkt, trade); // PV using CRR Binomial Pricer

        // Calculate Black-Scholes PV for European options for comparison
        if (res.isEuropeanOption) {
            auto euroOpt = static_cast<EuropeanOption*>(trade.get()); // Safe cast due to isEuropeanOption flag
            double T_days = (euroOpt->GetExpiry() - mkt->asOf);
            if (T_days > 0) {
                double T_years = T_days / 365.0;
                double S0 = mkt->getStockPrice(euroOpt->getUnderlying());
                std::shared_ptr<VolCurve> vc = mkt->getVolCurve("LOGVOL");
                double sigma = vc->getVol(euroOpt->GetExpiry());
                std::shared_ptr<RateCurve> rc_opt = mkt->getCurve(euroOpt->getRateCurve()); // Use option's rate curve
                double r = rc_opt->getRate(euroOpt->GetExpiry());

                res.blackScholesPv = BlackScholes::PriceEuropeanOption(S0, euroOpt->getStrike(), T_years, r, sigma, euroOpt->getOptType()) * euroOpt->getNotional();
            } else {
                 res.blackScholesPv = euroOpt->Payoff(mkt->getStockPrice(euroOpt->getUnderlying())) * euroOpt->getNotional(); // Intrinsic if expired
            }
        }
		results.push_back(res);
	}
    cout << "Portfolio pricing complete." << endl << endl;

	// Step 4: Compute Greeks (DV01, Vega)
    cout << "Calculating Greeks (DV01, Vega)..." << endl;
	double curve_shock_val = 0.0001; // 1 bp
	double vol_shock_val = 0.01;   // 1%
	double price_shock_val = 1.0;  // $1 (not used for DV01/Vega currently)
	RiskEngine risk_engine(*mkt, curve_shock_val, vol_shock_val, price_shock_val);

    bool use_multithreading_for_risk = false; // Set to true to test async risk calculation

	for (size_t i = 0; i < myPortfolio.size(); i++) {
		auto& trade = myPortfolio[i];
        // cout << "Calculating DV01 for trade " << results[i].id_num << " (" << results[i].tradeInfo << ")" << endl;
		risk_engine.computeRisk(trade, "DV01", pricer, !use_multithreading_for_risk);
		results[i].dv01_map = risk_engine.getResult();
        for(const auto& pair : results[i].dv01_map) results[i].total_dv01 += pair.second;


        // Only calculate Vega for options
        if (dynamic_cast<TreeProduct*>(trade.get())) {
            // cout << "Calculating Vega for trade " << results[i].id_num << " (" << results[i].tradeInfo << ")" << endl;
            risk_engine.computeRisk(trade, "VEGA", pricer, !use_multithreading_for_risk);
            results[i].vega_map = risk_engine.getResult();
            for(const auto& pair : results[i].vega_map) results[i].total_vega += pair.second;
        } else {
            results[i].total_vega = 0.0; // Vega is not applicable or zero for Bond/Swap
        }
	}
    cout << "Greek calculations complete." << endl << endl;

	// Step 5: Output Results to File
	outputResultToFile(results, "output_pv_greeks.txt");

    // Step 6: Generate Write-up
    generateWriteUp(results, *mkt);

	cout << endl << "Project tasks completed successfully!" << endl;
	return 0;
}

// Need to add getOptType() and getStrike() to EuropeanOption and AmericanOption (or their base TreeProduct)
// And getRateCurve() to TreeProduct for options to use their specified discount curve.
// Currently, EuropeanOption.h:
// protected: OptionType optType; double strike=0; string rateCurve;
// Need public getters for these.

// TreeProduct.h:
// class TreeProduct: public Trade { ... virtual const Date& GetExpiry() const = 0; ...};
// Add to TreeProduct.h:
// virtual OptionType getOptType() const = 0;
// virtual double getStrike() const = 0;
// virtual const std::string& getRateCurve() const = 0;

// EuropeanOption.h / AmericanOption.h implement these getters.
// EuropeanOption:
// OptionType getOptType() const override { return optType; }
// double getStrike() const override { return strike; }
// const std::string& getRateCurve() const override { return rateCurve; }
// (Similar for AmericanOption)

```

**Summary of `Main.cpp` changes:**
1.  **`trim` function:** Added a basic string trim utility.
2.  **`loadStockPrices` function:** Added to load stock prices from `stockPrice.txt`.
3.  **`loadTrade` function:**
    *   Normalized `type`, `optionTypeStr`, `directionStr` to lowercase for robust parsing.
    *   Adjusted `notional_val` based on `directionStr` (pay/receive, long/short) for all relevant trade types.
    *   Added more error/warning messages for malformed lines or unknown types/directions.
4.  **`loadIrCurve` / `loadVolCurve`:** Added trim and '%' removal for rate/vol strings. Added basic error checking for line format.
5.  **`TradeResult` struct:** Modified to store `id_num`, `dv01_map`, `total_dv01`, `vega_map`, `total_vega`. Added fields for Black-Scholes PV comparison.
6.  **`main` function:**
    *   Set default float precision for output.
    *   Platform-specific `localtime_s` / `localtime_r`.
    *   Called `loadStockPrices`.
    *   **Pricing Loop:**
        *   Populates `res.tradeInfo` with more detail for options (type, strike).
        *   Sets `res.isEuropeanOption` flag.
        *   Calculates and stores Black-Scholes PV in `res.blackScholesPv` for European options. This uses the `BlackScholes::PriceEuropeanOption` function (which needs to be accessible; I've added a forward declaration, but it implies `Pricer.cpp` functions should be in a header or `Pricer.h` includes `BlackScholesPricer.h` which defines them). For now, I'll assume `Pricer.cpp` is compiled and linked.
    *   **Risk Calculation Loop:**
        *   Instantiates `RiskEngine`.
        *   Calls `risk_engine.computeRisk` for "DV01" and "VEGA", passing the `pricer`.
        *   Stores results from `risk_engine.getResult()` into `dv01_map` and `vega_map`.
        *   Calculates `total_dv01` and `total_vega` by summing map values.
        *   Vega calculation is skipped for non-options.
        *   `use_multithreading_for_risk` flag added to easily toggle threading for RiskEngine.
7.  **`outputResultToFile` function:** Updated to use new `TradeResult` members and write BS PV.
8.  **`generateWriteUp` function:** Added a basic structure to generate `write_up.txt` with comparisons as required. This part is illustrative and might need more data/trade-specific comparisons.

**Further required changes (outside `Main.cpp` but highlighted by its usage):**
*   `TreeProduct` (in `TreeProduct.h`) needs public virtual getters: `getOptType()`, `getStrike()`, `getRateCurve()`.
*   `EuropeanOption` and `AmericanOption` (in `EuropeanTrade.h`, `AmericanTrade.h`) need to override and implement these getters.
*   The `BlackScholes::PriceEuropeanOption` function (currently in `Pricer.cpp`) should ideally be declared in a header (e.g., `BlackScholesPricer.h` or similar) and included by `Main.cpp` if `Main.cpp` calls it directly. Or, it can remain in `Pricer.cpp` and `Main.cpp` can just link to it. The current forward declaration in `Main.cpp` is a temporary solution.

I will first commit the `Main.cpp` changes. Then I'll make the necessary changes to `TreeProduct.h`, `EuropeanTrade.h`, and `AmericanTrade.h`.
