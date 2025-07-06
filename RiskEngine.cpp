#include "RiskEngine.h"
#include "Pricer.h" // For std::shared_ptr<Pricer>
#include <string>   // For std::string
#include <memory>   // For std::shared_ptr
#include <vector>   // For std::vector (used in function body)
#include <future>   // For std::future, std::async, std::launch (used in function body)
#include <utility>  // For std::make_pair (used in function body)

// Declaration from RiskEngine.h:
// void RiskEngine::computeRisk(std::shared_ptr<Trade> trade, const std::string& riskType, std::shared_ptr<Pricer> pricer, bool singleThread);
void RiskEngine::computeRisk(std::shared_ptr<Trade> trade, const std::string& riskType, std::shared_ptr<Pricer> pricer, bool singleThread)
{
    // Note: The 'pricer' argument is new and currently unused in this function body as per prior discussion.
    // The logic for "vega" and async tasks needs further review regarding VolDecorator methods
    // and how 'pricer' should be integrated if it's intended to be used for these calculations.
    result.clear();
    if (singleThread) {
        if (riskType == "dv01") {
            for (auto& kv : curveShocks) {
                std::string market_id = kv.first;
                auto mkt_u = kv.second.getMarketUp();
                auto mkt_d = kv.second.getMarketDown();
                double pv_up = trade->Pv(mkt_u); // Pv method is on Trade, not Pricer
                double pv_down = trade->Pv(mkt_d); // Pv method is on Trade, not Pricer
                double dv01 = (pv_up - pv_down) / 2.0;
                result.emplace(market_id, dv01);
            }
        }

        if (riskType == "vega") {
            for (auto& kv : volShocks) {
                std::string market_id = kv.first;
                // WARNING: The following lines use getOriginMarket() and getMarket()
                // which are not present in the VolDecorator class as per RiskEngine.h.
                // VolDecorator has getMarketUp() and getMarketDown().
                // This will likely lead to compilation errors or incorrect logic.
                // This logic needs to be revised based on available methods and intended vega calculation.
                auto mkt = kv.second.getOriginMarket(); // POTENTIAL ISSUE: Method may not exist
                auto mkt_s = kv.second.getMarket();   // POTENTIAL ISSUE: Method may not exist
                double pv = trade->Pv(mkt); // Pv method is on Trade
                double pv_up = trade->Pv(mkt_s); // Pv method is on Trade
                double vega_val = (pv_up - pv); // Original calculation for vega was one-sided
                result.emplace(market_id, vega_val);
            }
        }

        if (riskType == "price") {
            // to be added
        }
    }
    else { // Multithreaded
        // Lambda for DV01 type calculations
        auto pv_task_dv01 = [](std::shared_ptr<Trade> trade_param, const Market& mkt_up_param, const Market& mkt_down_param) { 
            double pv_up = trade_param->Pv(mkt_up_param);
            double pv_down = trade_param->Pv(mkt_down_param);
            return (pv_up - pv_down) / 2.0;
        };

        // Lambda for Vega type calculations (one-sided shock)
        auto pv_task_vega = [](std::shared_ptr<Trade> trade_param, const Market& mkt_base_param, const Market& mkt_shocked_param) {
            double pv_base = trade_param->Pv(mkt_base_param);
            double pv_shocked = trade_param->Pv(mkt_shocked_param);
            return pv_shocked - pv_base;
        };

        std::vector<std::future<std::pair<std::string, double>>> futures_vec; // Renamed to avoid conflict

        if (riskType == "dv01") {
            for (auto& shock : curveShocks) {
                std::string market_id = shock.first;
                const auto& mkt_u = shock.second.getMarketUp(); 
                const auto& mkt_d = shock.second.getMarketDown(); 
                futures_vec.push_back(std::async(std::launch::async, [trade, mkt_u, mkt_d, market_id, pv_task_dv01](){
                    return std::make_pair(market_id, pv_task_dv01(trade, mkt_u, mkt_d));
                }));
            }
        } else if (riskType == "vega") {
            for (auto& shock : volShocks) {
                std::string market_id = shock.first;
                // WARNING: Problematic logic for vega async due to getOriginMarket/getMarket
                // Assuming for now that mkt represents base and mkt_s represents shocked for the lambda.
                // This requires VolDecorator to have these methods, which is currently not the case.
                const auto& mkt_base_for_vega = shock.second.getOriginMarket(); // POTENTIAL ISSUE
                const auto& mkt_shocked_for_vega = shock.second.getMarket();    // POTENTIAL ISSUE
                futures_vec.push_back(std::async(std::launch::async, [trade, mkt_base_for_vega, mkt_shocked_for_vega, market_id, pv_task_vega](){
                    return std::make_pair(market_id, pv_task_vega(trade, mkt_base_for_vega, mkt_shocked_for_vega));
                }));
            }
        }

        for (auto&& fut : futures_vec) {
            result.emplace(fut.get()); 
        }
    }
}
