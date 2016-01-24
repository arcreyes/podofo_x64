#pragma once

#include <string>

#include "VdpObject.h"
#include "VdpTextState.h"
#include "VdpColor.h"

class VdpText : public VdpObject
{
public:
	VdpText();
	VdpText(std::string rawData, bool bArray, double xpos, double ypos, VdpTextState textState, VdpGraphicsState graphState, VdpRect boundingBox);
	~VdpText();

	inline const std::string & GetRawData(void)
	{
		return m_rawData;
	}

	bool operator==(VdpText & other)
	{
		return (*this).m_rawData == other.m_rawData &&
			(*this).m_bArray == other.m_bArray &&
			(*this).m_textMatrix == other.m_textMatrix &&
			(*this).m_xpos == other.m_xpos &&
			(*this).m_ypos == other.m_ypos;
	}

	bool operator!=(VdpText & other)
	{
		return !((*this) == other);
	}

protected:
	std::string	m_rawData;
	int			m_length;
	bool		m_bArray;

	double m_xpos;
	double m_ypos;
	VdpMatrix m_textMatrix;
	VdpTextState m_textState;
};

