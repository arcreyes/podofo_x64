#include "TextReader.h"
#include "PdfPainterEx.h"

#include <stack>

using namespace std;

TextReader::TextReader( const char *pszFileName )
{
	if (!pszFileName)
	{
		PODOFO_RAISE_ERROR(ePdfError_InvalidHandle);
	}

	m_pDoc = new PdfMemDocument(pszFileName);
	
}


TextReader::~TextReader()
{
	if (m_pDoc)
		delete m_pDoc;

}

void TextReader::CopyPagesToFile(const char *pszFilename, int iStart, int iCount)
{
	PdfMemDocument doc;

	doc.InsertPages(*m_pDoc, iStart, iCount);

	doc.Write(pszFilename);
}

int TextReader::StartExtract(DocObjects *pDocObjects, int count)
{
	int nCount = m_pDoc->GetPageCount();

	//if (nCount > 10)
	//{
	//	nCount = 10;
	//}

	for ( int i = 0; i < nCount; i++ )
	{
		PdfPage* pPage = m_pDoc->GetPage( i );

		PageObjects *pPageObjects = new PageObjects;

		printf("extracting page %d...", i);
		this->ExtractObjects(pPage, pPageObjects);
		printf("done\n");

		pDocObjects->push_back(pPageObjects);
	}

	return AnalyzeVariableData(pDocObjects, count);
}

void TextReader::ExtractObjects(PdfPage* pPage, PageObjects *pPageObjects)
{
	const char*      pszToken = NULL;
	PdfVariant       var;
	EPdfContentsType eType;

	PdfContentsTokenizer tokenizer(pPage);

	double dCurPosX = 0.0;
	double dCurPosY = 0.0;

	bool   bTextBlock = false;

	double fontSize = 0.0;
	PdfFont* pCurFont = NULL;
	PdfName fontName;

	Matrix Tm;
	Matrix cm;

	double dTL = 0.0;

	std::stack<PdfVariant> stack;
	std::stack<Matrix> stackCm;

	double Tw = 0.0;
	double Tc = 0.0;

	while (tokenizer.ReadNext(eType, pszToken, var))
	{
		if (eType == ePdfContentsType_Keyword)
		{
			// support 'l' and 'm' tokens
			if (strcmp(pszToken, "l") == 0 ||
				strcmp(pszToken, "m") == 0)
			{
				dCurPosX = stack.top().GetReal();
				stack.pop();
				dCurPosY = stack.top().GetReal();
				stack.pop();
			}
			else if (strcmp(pszToken, "BT") == 0)
			{
				bTextBlock = true;
			}
			else if (strcmp(pszToken, "ET") == 0)
			{
				if (!bTextBlock)
					fprintf(stderr, "WARNING: Found ET without BT!\n");

				bTextBlock = false;
			}
			else if (strcmp(pszToken, "q") == 0)
			{
				stackCm.push(cm);
			}
			else if (strcmp(pszToken, "Q") == 0)
			{
				cm = stackCm.top();
				stackCm.pop();
			}
			else if (strcmp(pszToken, "cm") == 0)
			{
				double a, b, c, d, e, f;
				f= stack.top().GetReal();
				stack.pop();
				e = stack.top().GetReal();
				stack.pop();
				d = stack.top().GetReal();
				stack.pop();
				c = stack.top().GetReal();
				stack.pop();
				b = stack.top().GetReal();
				stack.pop();
				a = stack.top().GetReal();
				stack.pop();

				cm.SetMatrix(a, b, c, d, e, f);
			}

			else if (strcmp(pszToken, "Do") == 0)
			{
				PdfName imageName = stack.top().GetName();
				stack.pop();

				PdfObject *pObj = pPage->GetFromResources(PdfName("XObject"), imageName);
				if (!pObj)
				{
					PODOFO_RAISE_ERROR_INFO(ePdfError_InvalidHandle, "Cannot create font!");
				}

				PdfObject *pObjSubType = pObj->GetDictionary().GetKey(PdfName::KeySubtype);
				if (pObjSubType && pObjSubType->IsName() && 
					(pObjSubType->GetName().GetName() == "Image"))
				{
					ImageObj *pObj = new ImageObj(imageName, cm);

					pPageObjects->push_back(pObj);
				}

			}

			if (bTextBlock)
			{
				if (strcmp(pszToken, "Tf") == 0)
				{
					fontSize = stack.top().GetReal();
					stack.pop();

					fontName = stack.top().GetName();
					PdfObject* pFont = pPage->GetFromResources(PdfName("Font"), fontName);
					if (!pFont)
					{
						PODOFO_RAISE_ERROR_INFO(ePdfError_InvalidHandle, "Cannot create font!");
					}

					pCurFont = m_pDoc->GetFont(pFont);
					if (!pCurFont)
					{
						fprintf(stderr, "WARNING: Unable to create font for object %i %i R\n",
							pFont->Reference().ObjectNumber(),
							pFont->Reference().GenerationNumber());
					}

					//printf("\n/%s %g Tf\n", fontName.GetName().c_str(), fontSize);
					//printf("Font Size       : %g\n", pCurFont->GetFontSize());
					//printf("Font Scale      : %g\n", pCurFont->GetFontScale());
					//printf("Font Char Space : %g\n", pCurFont->GetFontCharSpace());
					//printf("Font Word Space : %g\n", pCurFont->GetWordSpace());
					//printf("Bold            : %s\n", pCurFont->IsBold() ? "TRUE" : "FALSE" );
					//printf("Italic          : %s\n", pCurFont->IsItalic() ? "TRUE" : "FALSE");
					//printf("Underlined      : %s\n", pCurFont->IsUnderlined() ? "TRUE" : "FALSE");
					//printf("Ascent          : %g\n", pCurFont->GetFontMetrics()->GetAscent());
					//printf("Descent         : %g\n", pCurFont->GetFontMetrics()->GetDescent());
				}
				else if (strcmp(pszToken, "'") == 0)
				{
					Tm.Translate(0, dTL);

					TextObj *pObj = new TextObj(stack.top().GetString(), pCurFont, fontSize, fontName, Tm, Tw, Tc);
					stack.pop();

					pPageObjects->push_back(pObj);
				}
				else if (strcmp(pszToken, "Tj") == 0)
				{
					pCurFont->SetFontSize((float)(fontSize * Tm.GetA()));
					pCurFont->SetFontCharSpace((float)(Tc * Tm.GetA()));
					pCurFont->SetWordSpace((float)(Tw * Tm.GetA()));

					PdfString unicodeString = pCurFont->GetEncoding()->ConvertToUnicode(stack.top().GetString(), pCurFont);						
					stack.pop();

					TextObj *pObj = new TextObj(unicodeString, pCurFont, fontSize, fontName, Tm, Tw, Tc);

					printf("%s\n", unicodeString.GetStringUtf8().c_str());

					pObj->SetBoundingBox(Tm.GetE(),
						Tm.GetF() + pCurFont->GetFontMetrics()->GetDescent(),
						pCurFont->GetFontMetrics()->StringWidth(unicodeString.GetUnicode(), unicodeString.GetUnicodeLength()),
						pCurFont->GetFontMetrics()->GetAscent() - pCurFont->GetFontMetrics()->GetDescent());


					pPageObjects->push_back(pObj);
				}
				else if (strcmp(pszToken, "\"") == 0)
				{
					TextObj *pObj = new TextObj(stack.top().GetString(), pCurFont, fontSize, fontName, Tm, Tw, Tc);
					stack.pop();

					Tc = stack.top().GetReal();
					stack.pop(); // remove char spacing from stack
					Tw = stack.top().GetReal();
					stack.pop(); // remove word spacing from stack

					pObj->SetCharacterSpacing(Tc);
					pObj->SetWordSpacing(Tw);

					Tm.Translate(0, dTL);

					pPageObjects->push_back(pObj);
				}
				else if (strcmp(pszToken, "TJ") == 0)
				{
					PdfArray pdfArray = stack.top().GetArray();
					stack.pop();

					pCurFont->SetFontSize((float)(fontSize * Tm.GetA()));
					pCurFont->SetFontCharSpace((float)(Tc * Tm.GetA()));
					pCurFont->SetWordSpace((float)(Tw * Tm.GetA()));

					double dTJOffset = Tm.GetE();

					for (int i = 0; i < pdfArray.size(); i++)
					{
						if (pdfArray[i].IsNumber())
						{
							dTJOffset = (dTJOffset - ((pdfArray[i].GetNumber() / 1000.0) * Tm.GetA()));
						}
						else if (pdfArray[i].IsString() || pdfArray[i].IsHexString())
						{

							dTJOffset += pCurFont->GetFontMetrics()->StringWidth(pdfArray[i].GetString());
						}
					}


					TextObj *pObj = new TextObj(pdfArray, pCurFont, fontSize, fontName, Tm, Tw, Tc);

					pObj->SetBoundingBox(Tm.GetE(),
						Tm.GetF() + pCurFont->GetFontMetrics()->GetDescent(),
						dTJOffset - Tm.GetE(),
						pCurFont->GetFontMetrics()->GetAscent() - pCurFont->GetFontMetrics()->GetDescent());

					pPageObjects->push_back(pObj);

				}
				else if (strcmp(pszToken, "Tm") == 0)
				{
					double a, b, c, d, e, f;
					f = stack.top().GetReal();
					stack.pop();
					e = stack.top().GetReal();
					stack.pop();
					d = stack.top().GetReal();
					stack.pop();
					c = stack.top().GetReal();
					stack.pop();
					b = stack.top().GetReal();
					stack.pop();
					a = stack.top().GetReal();
					stack.pop();

					Tm.SetMatrix(a, b, c, d, e, f);

				}
				else if (strcmp(pszToken, "Td") == 0)
				{
					double dTy = stack.top().GetReal();
					stack.pop();
					double dTx = stack.top().GetReal();
					stack.pop();

					Tm.Translate(dTx, dTy);
				}
				else if (strcmp(pszToken, "TD") == 0)
				{
					double dTy = stack.top().GetReal();
					stack.pop();
					double dTx = stack.top().GetReal();
					stack.pop();

					dTL = dTy;

					Tm.Translate(dTx, dTy);
				}
				else if (strcmp(pszToken, "T*") == 0)
				{
					Tm.Translate(0, dTL);
				}
				else if (strcmp(pszToken, "Tw") == 0)
				{
					Tw = stack.top().GetReal();
					stack.pop(); // remove word spacing from stack
				}
				else if (strcmp(pszToken, "Tc") == 0)
				{
					Tc = stack.top().GetReal();
					stack.pop(); // remove char spacing from stack
				}
			}
		}
		else if (eType == ePdfContentsType_Variant)
		{
			stack.push(var);
		}
		else
		{
			// Impossible; type must be keyword or variant
			PODOFO_RAISE_ERROR(ePdfError_InternalLogic);
		}
	}

}

int TextReader::AnalyzeVariableData(DocObjects *pDocObjects, int count)
{
	int pagecount = (int)pDocObjects->size();

	if (pagecount < 2)
	{
		return -1;
	}

	for (int i = 0; i < count; i++)
	{
		PageObjects *pBasePageObjects = pDocObjects->at(i);

		std::stack<ObjectBase *> stack;

		for (int j = 0; j < (int)pBasePageObjects->size(); j++)
		{
			ObjectBase *pBaseObj = pBasePageObjects->at(j);

			printf("Analyzing object %d of %d...\n", j, pBasePageObjects->size());

			int k;
			for (k = count; k < pagecount; k += count)
			{
				PageObjects *pComparePageObjects = pDocObjects->at(k);

				int l;
				for (l = 0; l < (int)pComparePageObjects->size(); l++)
				{
					ObjectBase *pCompareObj = pComparePageObjects->at(l);

					if (*pBaseObj == *pCompareObj)
					{
						if (pBaseObj->GetObjectType() == eTextObj)
						{
							TextObj *pBaseTextObj = static_cast<TextObj*>(pBaseObj);
							TextObj *pCompareTextObj = static_cast<TextObj*>(pCompareObj);

							//printf("%s vs %s...", pBaseTextObj->GetValue().c_str(), pCompareTextObj->GetValue().c_str());

							if (*pBaseTextObj == *pCompareTextObj)
							{
								//printf("same\n");
								stack.push(pCompareTextObj);
								break;
							}
							else
							{
								//printf("different\n");
 							}
						}
						else if (pBaseObj->GetObjectType() == eImageObj)
						{
							ImageObj *pBaseImageObj = static_cast<ImageObj*>(pBaseObj);
							ImageObj *pCompareImageObj = static_cast<ImageObj*>(pCompareObj);

							if (*pBaseImageObj == *pCompareImageObj)
							{
								stack.push(pCompareImageObj);
								break;
							}
						}
						else
						{
							return -1;
						}
					}
				}

				// no match found so break
				if (l >= (int)pComparePageObjects->size())
				{
					break;
				}
			}

			// matched at every page
			if (k >= pagecount)
			{
				pBaseObj->SetConstant();

				while (!stack.empty())
				{
					stack.top()->SetConstant();
					stack.pop();
				}
			}
			else
			{
				while (!stack.empty())
				{
					stack.pop();
				}
			}
			printf("Analyzing object %d of %d...done\n", j, pBasePageObjects->size());
		}
	}

	return 0;
}

void TextReader::AddTextElement(double dCurPosX, double dCurPosY,
	PdfFont* pCurFont, const PdfString & rString)
{
	if (!pCurFont)
	{
		fprintf(stderr, "WARNING: Found text but do not have a current font: %s\n", rString.GetString());
		return;
	}

	if (!pCurFont->GetEncoding())
	{
		fprintf(stderr, "WARNING: Found text but do not have a current encoding: %s\n", rString.GetString());
		return;
	}

	// For now just write to console
	PdfString unicode = pCurFont->GetEncoding()->ConvertToUnicode(rString, pCurFont);
	const char* pszData = unicode.GetStringUtf8().c_str();
	while (*pszData) {
		//printf("%02x", static_cast<unsigned char>(*pszData) );
		++pszData;
	}
	//printf("(%.3f,%.3f) %s \n", dCurPosX, dCurPosY, unicode.GetStringUtf8().c_str());
}

void TextReader::CreateTemplate(const char *pszFileName, int count)
{
	PdfMemDocument document;

	PdfPage *pPageTemplate;
	PdfPage *pPageOriginal;

	try
	{
		for (int i = 0; i < count; i++)
		{
			const char*      pszToken = NULL;
			PdfVariant       var;
			EPdfContentsType eType;

			pPageOriginal = m_pDoc->GetPage(i);
			PdfContentsTokenizer tokenizer(pPageOriginal);

			bool   bTextBlock = false;
			PdfFont* pCurFont = NULL;

			PdfRefCountedBuffer buffer;
			PdfOutputDevice device(&buffer);

			std::vector<PdfVariant> args;

			while (tokenizer.ReadNext(eType, pszToken, var))
			{
				if (eType == ePdfContentsType_Keyword)
				{
					if (strcmp(pszToken, "BT") == 0)
					{
						bTextBlock = true;
						// BT does not reset font
						// pCurFont     = NULL;
					}
					else if (strcmp(pszToken, "ET") == 0)
					{
						if (!bTextBlock)
							fprintf(stderr, "WARNING: Found ET without BT!\n");
					}

					if (bTextBlock)
					{
						if (strcmp(pszToken, "Tj") == 0 ||
							strcmp(pszToken, "'") == 0)
						{
							//AddTextElement(dCurPosX, dCurPosY, pCurFont, args.back().GetString());

						}
						else if (strcmp(pszToken, "\"") == 0)
						{
							//AddTextElement(dCurPosX, dCurPosY, pCurFont, args.back().GetString());
	
						}
						else if (strcmp(pszToken, "TJ") == 0)
						{
							PdfArray array = args.back().GetArray();

							std::string value;

							for (int i = 0; i<static_cast<int>(array.GetSize()); i++)
							{
								if (array[i].IsString() || array[i].IsHexString())
									value += array[i].GetString().GetString();
							}
						}
					}

					WriteArgumentsAndKeyword(args, pszToken, device);
				}
				else if (eType == ePdfContentsType_Variant)
				{
					args.push_back(var);
				}
				else
				{
					// Impossible; type must be keyword or variant
					PODOFO_RAISE_ERROR(ePdfError_InternalLogic);
				}
			}


			WriteArgumentsAndKeyword(args, NULL, device);

			pPageTemplate = document.CreatePage(pPageOriginal->GetPageSize());
			if (!pPageTemplate)
			{
				PODOFO_RAISE_ERROR(ePdfError_InvalidHandle);
			}
			pPageTemplate->GetContentsForAppending()->GetStream()->Set(buffer.GetBuffer(), buffer.GetSize());

			m_mapMigrate.clear();
			TKeyMap resmap = pPageOriginal->GetResources()->GetDictionary().GetKeys();
			for (TCIKeyMap itres = resmap.begin(); itres != resmap.end(); ++itres)
			{
				PdfObject *o = itres->second;
				pPageTemplate->GetResources()->GetDictionary().AddKey(itres->first, MigrateResource(o, &document, m_pDoc));
			}
		}


		document.GetInfo()->SetCreator(PdfString("examplahelloworld - A PoDoFo test application"));
		document.GetInfo()->SetAuthor(PdfString("Dominik Seichter"));
		document.GetInfo()->SetTitle(PdfString("Hello World"));
		document.GetInfo()->SetSubject(PdfString("Testing the PoDoFo PDF Library"));
		document.GetInfo()->SetKeywords(PdfString("Test;PDF;Hello World;"));

		/*
		* The last step is to close the document.
		*/
		document.Write(pszFileName);
	}
	catch (const PdfError & e) {
		/*
		* All PoDoFo methods may throw exceptions
		* make sure that painter.FinishPage() is called
		* or who will get an assert in its destructor
		*/
		try {
			//painter.FinishPage();
		}
		catch (...) {
			/*
			* Ignore errors this time
			*/
		}

		throw e;
	}
}

void TextReader::PrintVariableData(DocObjects *pDocObjects)
{
	int objPageCount = (int)pDocObjects->size();
	PageObjects *pPageObjects;

	for (int i = 0; i < objPageCount; i++)
	{
		printf("page %d --------\n", i);

		pPageObjects = pDocObjects->at(i);

		for (int j = 0; j < (int)pPageObjects->size(); j++)
		{
			ObjectBase *pObj = pPageObjects->at(j);

			if (pObj->IsVariableData())
			{
				if (pObj->GetObjectType() == eTextObj)
				{
					TextObj *pTextObj = static_cast<TextObj *>(pObj);

					printf("%s\n", pTextObj->GetValue().c_str());
				}
			}
		}
	}
}

void TextReader::MarkText(const char *pszFileName, DocObjects *pDocObjects)
{
	int objPageCount = (int)pDocObjects->size();
	int pageCount = m_pDoc->GetPageCount();

	PdfPage *pPage = NULL;
	PdfPainter painter;
	PageObjects *pPageObjects;

	for (int i = 0; i < objPageCount && i < pageCount; i++)
	{
		pPage = m_pDoc->GetPage(i);
		pPageObjects = pDocObjects->at(i);

		printf("marking page %d of %d...", i, objPageCount);

		painter.SetPage(pPage);

		painter.SetStrokeWidth(0.05);
		

		for (int j = 0; j < (int)pPageObjects->size(); j++)
		{
			ObjectBase *pObj = pPageObjects->at(j);

			//if (pObj->IsVariableData())
			//{
				if (pObj->GetObjectType() == eTextObj)
				{
					painter.SetStrokingColor(PdfColor(1.0, 0.0, 0.0));
					TextObj *pTextObj = static_cast<TextObj*>(pObj);

					double dX = pTextObj->GetPosX();
					double dY = pTextObj->GetPosY();
					double dWidth = pTextObj->GetWidth();
					double dHeight = pTextObj->GetHeight();
					painter.MoveTo(dX, dY);
					painter.LineTo(dX + dWidth, dY);
					painter.LineTo(dX + dWidth, dY + dHeight);
					painter.LineTo(dX, dY + dHeight);
					painter.ClosePath();

					painter.Stroke();
				}
				else
				{
					painter.SetStrokingColor(PdfColor(0.0, 1.0, 0.0));
					ImageObj *pImageObj = static_cast<ImageObj*>(pObj);
					Matrix cm = pImageObj->GetTransformMatrix();

					painter.Rectangle(cm.GetE(), cm.GetF(), cm.GetA(), cm.GetD());

					painter.Stroke();
				}
			//}
		}


		painter.FinishPage();

		printf("done\n");

	}

	m_pDoc->Write(pszFileName);
}

void TextReader::WriteArgumentsAndKeyword(std::vector<PdfVariant> & rArgs, const char* pszKeyword, PdfOutputDevice & rDevice)
{
	std::vector<PdfVariant>::const_iterator it = rArgs.begin();
	while (it != rArgs.end())
	{
		(*it).Write(&rDevice, ePdfWriteMode_Compact);
		++it;
	}

	rArgs.clear();

	if (pszKeyword)
	{
		rDevice.Write(" ", 1);
		rDevice.Write(pszKeyword, strlen(pszKeyword));
		rDevice.Write("\n", 1);
	}
}

PdfObject* TextReader::MigrateResource(PdfObject * obj, PdfMemDocument *pTargetDoc, PdfMemDocument *pSourceDoc)
{
	PdfObject *ret(0);

	if (obj->IsDictionary())
	{
		ret = pTargetDoc->GetObjects().CreateObject(*obj);

		TKeyMap resmap = obj->GetDictionary().GetKeys();
		for (TCIKeyMap itres = resmap.begin(); itres != resmap.end(); ++itres)
		{
			PdfObject *o = itres->second;
			ret->GetDictionary().AddKey(itres->first, MigrateResource(o, pTargetDoc, pSourceDoc));
		}

		if (obj->HasStream())
		{
			*(ret->GetStream()) = *(obj->GetStream());
		}
	}
	else if (obj->IsArray())
	{
		PdfArray carray(obj->GetArray());
		PdfArray narray;
		for (unsigned int ci = 0; ci < carray.GetSize(); ++ci)
		{
			PdfObject *co(MigrateResource(&carray[ci], pTargetDoc, pSourceDoc));
			narray.push_back(*co);
		}
		ret = pTargetDoc->GetObjects().CreateObject(narray);
	}
	else if (obj->IsReference())
	{
		if (m_mapMigrate.find(obj->GetReference().ToString()) != m_mapMigrate.end())
		{
			return m_mapMigrate[obj->GetReference().ToString()];
		}

		PdfObject * o(MigrateResource(pSourceDoc->GetObjects().GetObject(obj->GetReference()), pTargetDoc, pSourceDoc));

		ret = new PdfObject(o->Reference());

	}
	else
	{
		ret = new PdfObject(*obj);//targetDoc->GetObjects().CreateObject(*obj);
	}


	m_mapMigrate.insert(std::pair<std::string, PdfObject*>(obj->Reference().ToString(), ret));


	return ret;

}