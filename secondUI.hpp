#ifndef SECONDUI_H_INCLUDED
#define SECONDUI_H_INCLUDED

#include <uf_defs.h>
#include <iostream>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_Group.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyPoint.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_BlockFeatureBuilder.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Update.hxx>

using namespace std;
using namespace NXOpen;
using namespace NXOpen::BlockStyler;

class DllExport secondUI
{
public:
    static Session *theSession;
    static UI *theUI;
    secondUI();
    ~secondUI();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();
    void initialize_cb();
    void dialogShown_cb();
    int apply_cb();
    int ok_cb();
    int cancel_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    PropertyList* GetBlockProperties(const char *blockID);

private:
    const char* theDlxFileName;
    NXOpen::BlockStyler::BlockDialog* theDialog;
    NXOpen::BlockStyler::Group* group3;
    NXOpen::BlockStyler::Button* button0;
    NXOpen::BlockStyler::Button* button01;
    NXOpen::BlockStyler::Group* group2;
    NXOpen::BlockStyler::SelectObject* selection0;
    NXOpen::BlockStyler::Group* group1;
    NXOpen::BlockStyler::SpecifyPoint* point0;
    NXOpen::BlockStyler::Enumeration* enum0;
    NXOpen::BlockStyler::Group* group;
    NXOpen::BlockStyler::SpecifyPoint* point01;
    NXOpen::BlockStyler::Enumeration* enum01;
    int m_opMode;
};
#endif