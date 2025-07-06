#include "Date.h"

long Date::getSerialDate() const
{	//1900-1-1 ->1
	// Adjust for Excel's incorrect leap year handling (Excel treats 1900 as a leap year)
	int daysSinceEpoch = 0;
	for (int y = 1900; y < year; ++y) {
		daysSinceEpoch += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
	}

	int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
		daysInMonth[1] = 29; // Leap year adjustment
	}

	for (int m = 0; m < month - 1; ++m) {
		daysSinceEpoch += daysInMonth[m];
	}

	daysSinceEpoch += day;

	// Excel incorrectly considers 1900 as a leap year, so we add 1 for compatibility
	if (year > 1900) {
		daysSinceEpoch += 1;
	}
	return daysSinceEpoch;
}
void Date::serialToDate(int serial)
{
	int daysSinceEpoch = serial-2; // Adjust for Excel's incorrect leap year handling
	int y = 1900;
	int m = 1;
	int d = 1;

	while (daysSinceEpoch > 0) {
		int daysInMonth;
		switch (m) {
		case 4: case 6: case 9: case 11: daysInMonth = 30; break;
		case 2: daysInMonth = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28; break;
		default: daysInMonth = 31;
		}

		if (daysSinceEpoch >= daysInMonth) {
			daysSinceEpoch -= daysInMonth;
			m++;
			if (m > 12) {
				m = 1;
				y++;
			}
		}
		else {
			d += daysSinceEpoch;
			break;
		}
	}

	year = y;
	month = m;
	day = d;

	serialNumber = serial;
}

Date dateAddTenor(const Date& start, const string& tenorStr)
{
	Date newdate;
	if (to_lower(tenorStr) == "on" || to_lower(tenorStr) == "o/n") {
		long newSerial = start.getSerialDate() + 1;
		newdate.serialToDate(newSerial);
	}
	else {
		int numUnit = stoi(tenorStr.substr(0, tenorStr.size() - 1));
		auto tenorUnit = tenorStr.back();
		if (tenorUnit == 'W') {
			long newSerial = start.getSerialDate() + numUnit * 7;
			newdate.serialToDate(newSerial);
		}
		else if (tenorUnit == 'M') {
			long newSerial = start.getSerialDate() + numUnit * 30;
			newdate.serialToDate(newSerial);
		}
		else if (tenorUnit == 'Y') {
			long newSerial = start.getSerialDate() + numUnit * 360;
			newdate.serialToDate(newSerial);
		}
		else
			throw std::runtime_error("Error: found unsupported tenor: " + tenorStr);
	}

	return newdate;
}
//this function return the year franction between 2 date object
double operator-(const Date& d1, const Date& d2)
{
	long numOfDays = d1.getSerialDate() - d2.getSerialDate();
	return numOfDays;
};
bool operator>(const Date& d1, const Date& d2)
{
	double gap = d1 - d2;
	if (gap > 0)
		return true;
	else
		return false;
};
bool operator<(const Date& d1, const Date& d2)
{
	return !(d1 > d2);
};
bool operator==(const Date& d1, const Date& d2)
{
	double gap = d1 - d2;
	if (gap == 0)
		return true;
	else
		return false;
};
std::ostream& operator<<(std::ostream& os, const Date& d)
{
	os << d.year << "-" << d.month << "-" << d.day;
	return os;
}
std::istream& operator>>(std::istream& is, Date& d)
{
	is >> d.year >> d.month >> d.day;
	return is;
}
