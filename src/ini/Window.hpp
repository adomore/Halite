
#pragma once

#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/vector.hpp>

class ListViewColumn
{
public:
	ListViewColumn() :
		index(0),
		isPresent(false)
	{}
	
	ListViewColumn(unsigned i, wstring n, bool p) :
		index(i),
		name(n),
		isPresent(p)
	{}
	
	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{	
		ar & BOOST_SERIALIZATION_NVP(index);
		ar & BOOST_SERIALIZATION_NVP(name);
		ar & BOOST_SERIALIZATION_NVP(isPresent);
	}	

	unsigned index;
	wstring name;
	bool isPresent;
};

namespace boost {
namespace serialization {



} // namespace serialization
} // namespace boost

class WindowConfig
{
public:
	WindowConfig() :
		splitterPos(100),
		use_tray(true),
		advancedUI(false)
	{
		rect.top = 10;
		rect.left = 10;
		rect.bottom = 400;
		rect.right = 620;
		
		mainListColWidth[0] = 100;
		mainListColWidth[1] = 110;
		mainListColWidth[2] = 60;
		mainListColWidth[3] = 60;
		mainListColWidth[4] = 60;
		mainListColWidth[5] = 42;
		mainListColWidth[6] = 45;
		
		mainListColWidthEx[0] = 45;
		mainListColWidthEx[1] = 45;	
		
		columnDetails.assign(9, ListViewColumn(-1, L"Default", false));
		columnDetails.push_back(ListViewColumn(0, L"One", true));
		columnDetails.push_back(ListViewColumn(2, L"Two", false));
	}
	
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int version)
	{	
		ar & BOOST_SERIALIZATION_NVP(rect);
		ar & BOOST_SERIALIZATION_NVP(splitterPos);
		ar & BOOST_SERIALIZATION_NVP(mainListColWidth);
		ar & BOOST_SERIALIZATION_NVP(use_tray);
		if (version > 0) {
			ar & BOOST_SERIALIZATION_NVP(mainListColWidthEx);
			ar & BOOST_SERIALIZATION_NVP(advancedUI);
		}
		if (version > 1) {
			ar & BOOST_SERIALIZATION_NVP(columnDetails);
		}
	}
	
	friend class GeneralOptions;
	friend class HaliteWindow;
	friend class HaliteListViewCtrl;
	friend int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
	
private:	
	static const size_t numMainCols = 7;
	static const size_t numMainColsEx = 2;
	CRect rect;
	unsigned int splitterPos;
	unsigned int mainListColWidth[numMainCols];
	unsigned int mainListColWidthEx[numMainColsEx];
	bool use_tray;
	bool advancedUI;
	
	std::vector<ListViewColumn> columnDetails;
};

BOOST_CLASS_VERSION(WindowConfig, 2)