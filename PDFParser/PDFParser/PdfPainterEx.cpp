#include "PdfPainterEx.h"


PdfPainterEx::PdfPainterEx()
{
}


PdfPainterEx::~PdfPainterEx()
{
}

void PdfPainterEx::SetTextTransformationMatrix(double a, double b, double c, double d, double e, double f)
{
	PODOFO_RAISE_LOGIC_IF(!m_pCanvas, "Call SetPage() first before doing drawing operations.");

	if (!m_pFont || !m_pPage || !m_isTextOpen)
	{
		PODOFO_RAISE_ERROR(ePdfError_InvalidHandle);
	}

	// Need more precision for transformation-matrix !!
	//std::streamsize oldPrecision = m_oss.precision(clPainterHighPrecision);
	m_oss.str("");
	m_oss << a << " "
		<< b << " "
		<< c << " "
		<< d << " "
		<< e << " "
		<< f << " Tm" << std::endl;
	//m_oss.precision(oldPrecision);

	m_pCanvas->Append(m_oss.str());
}

void PdfPainterEx::AddTextEx(const PdfString & sText)
{
	AddToPageResources(m_pFont->GetIdentifier(), m_pFont->GetObject()->Reference(), PdfName("Font"));

	AddText(sText);
}