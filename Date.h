#ifndef DATE_H
#define DATE_H

#include <iostream>
#include "helper.h"

class Date
{
public:
	Date() {};
	Date(int y, int m, int d) : year(y), month(m), day(d)
	{
		serialNumber = getSerialDate();
		if (serialNumber < 1)
			throw std::runtime_error("Error: invalid date, serial number is less than 1!");
	};
	Date(const string& strDate)
	{
		//format is "yyyy-mm-dd"
		year = stoi(strDate.substr(0, 4));
		month = stoi(strDate.substr(5, 7));
		day = stoi(strDate.substr(8, 10));
		serialNumber = getSerialDate();
	};

	long getSerialDate() const;
	void serialToDate(int serial);

	int year;
	int month;
	int day;
	int serialNumber = 1;
};

Date dateAddTenor(const Date& start, const string& tenorStr);

//this function return the year franction between 2 date object
double operator-(const Date& d1, const Date& d2);
bool operator>(const Date& d1, const Date& d2);
bool operator<(const Date& d1, const Date& d2);
bool operator==(const Date& d1, const Date& d2);
std::ostream& operator<<(std::ostream& os, const Date& d);
std::istream& operator>>(std::istream& is, Date& d);

#endif
