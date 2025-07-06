#include "Swap.h"
#include "Market.h"

void Swap::generateSchedule()
{
	if (startDate == maturityDate || frequency <= 0 || frequency > 1)
		throw std::runtime_error("Error: start date is later than end date, or invalid frequency!");

	string tenorStr;
	if (frequency == 0.25)
		tenorStr = "3M";
	else if (frequency == 0.5)
		tenorStr = "6M";
	else
		tenorStr = "1Y";

	Date seed = startDate;
	while (seed < maturityDate) {
		swapSchedule.push_back(seed);
		seed = dateAddTenor(seed, tenorStr);
	}
	swapSchedule.push_back(maturityDate);
	if (swapSchedule.size() < 2)
		throw std::runtime_error("Error: invalid schedule, check input!");

}
double Swap::Payoff(double s) const
{
	//this function will not be called
	return (s - tradeRate) * notional;
}
double Swap::getAnnuity(const Market& mkt) const
{
	double annuity = 0;	
	Date valueDate = mkt.asOf;
	auto rc = mkt.getCurve(rateCurve);
	for (size_t i = 1; i < swapSchedule.size(); i++) {
		auto dt = swapSchedule[i];
		if (dt < valueDate)
			continue;
		double tau = (swapSchedule[i] - swapSchedule[i - 1]) / 360;
		double df = rc->getDf(dt);
		annuity += notional * tau * df;
	}

	return annuity;
}
double Swap::Pv(const Market& mkt) const
{
	//using cash flow discunting
	Date valueDate = mkt.asOf;
	auto rc = mkt.getCurve(rateCurve);
	double df = rc->getDf(maturityDate);
	double fltPv = (notional - notional * df);
	double fixPv = 0;
	for (size_t i = 1; i < swapSchedule.size(); i++) {
		auto dt = swapSchedule[i];
		if (dt < valueDate)
			continue;
		double tau = (swapSchedule[i] - swapSchedule[i - 1]) / 360;
		df = rc->getDf(dt);
		fixPv += notional * tau * tradeRate * df;
	}

	return fixPv + fltPv;
}




