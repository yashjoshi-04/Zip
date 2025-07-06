#pragma once
#include <iostream>
#include "Swap.h"
#include "Bond.h"
#include "EuropeanTrade.h"
#include "AmericanTrade.h"

// Abstract creator class
class TradeFactory {
public:
	virtual std::shared_ptr<Trade> createTrade(std::string underlying, Date start, Date end, double notional, double strike, double freq, OptionType opt) = 0;
	virtual ~TradeFactory() {} // Virtual destructor for polymorphism
};

// Concrete creator class - SwapFactory
class SwapFactory : public TradeFactory {
public:
	std::shared_ptr<Trade> createTrade(std::string underlying, Date start, Date end, double notional, double strike, double freq, OptionType opt) override {
		return std::make_shared<Swap>(underlying, start, end, notional, strike, freq);//implement this
	}
};

// Concrete creator class - BondFactory
class BondFactory : public TradeFactory {
public:
	std::shared_ptr<Trade> createTrade(std::string underlying, Date start, Date end, double notional, double strike, double freq, OptionType opt) {
		return std::make_shared<Bond>(underlying, start, end, notional, strike, freq);// implement this
	}
};

// Concrete creator class - EuropeanFactory
class EurOptFactory : public TradeFactory {
public:
	std::shared_ptr<Trade> createTrade(std::string underlying, Date start, Date end, double notional, double strike, double freq, OptionType opt) {
		return make_shared<EuropeanOption>(opt, notional, strike, start, end, underlying);
	}
};

// Concrete creator class - AmericanOptFactory
class AmericanOptFactory : public TradeFactory {
public:
	std::shared_ptr<Trade> createTrade(std::string underlying, Date start, Date end, double notional, double strike, double freq, OptionType opt) {
		return std::make_shared<AmericanOption>(opt, notional, strike, start, end, underlying); //implement this
	}
};