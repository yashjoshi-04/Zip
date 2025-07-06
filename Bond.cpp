#include "Bond.h"
#include "Market.h"

void Bond::generateSchedule()
{
  //implement this
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
		bondSchedule.push_back(seed);
		seed = dateAddTenor(seed, tenorStr);
	}
	bondSchedule.push_back(maturityDate);
	if (bondSchedule.size() < 2)
		throw std::runtime_error("Error: invalid schedule, check input!");

}
double Bond::Payoff(double s) const
{
  double pv = notional * (s - tradePrice);
  return pv;
}
double Bond::Pv(const Market& mkt) const
{
  //using cash flow discunting
  // implement this
  return 0;
}



