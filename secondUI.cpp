#include "secondUI.hpp"
#include <sstream>
#include <cmath>

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

Session *(secondUI::theSession) = NULL;
UI *(secondUI::theUI) = NULL;

static void Log(const char *msg)
{
    auto s = Session::GetSession();
    if (s) { s->ListingWindow()->Open(); s->ListingWindow()->WriteLine(msg); }
}

static Body *GetBody(NXOpen::BlockStyler::SelectObject *sel)
{
    if (!sel) return nullptr;
    auto objs = sel->GetSelectedObjects();
    if (objs.empty()) return nullptr;
    // Try Body directly, then Face/Edge -> Body
    if (auto b = dynamic_cast<Body*>(objs[0])) return b;
    if (auto f = dynamic_cast<Face*>(objs[0])) return f->GetBody();
    if (auto e = dynamic_cast<Edge*>(objs[0])) return e->GetBody();
    return nullptr;
}//======================================================================
secondUI::secondUI()
{
    try
    {
        theSession = Session::GetSession();
        theUI = UI::GetUI();
        theDlxFileName = "secondUI.dlx";
        theDialog = theUI->CreateDialog(theDlxFileName);
        theDialog->AddApplyHandler(make_callback(this, &secondUI::apply_cb));
        theDialog->AddOkHandler(make_callback(this, &secondUI::ok_cb));
        theDialog->AddUpdateHandler(make_callback(this, &secondUI::update_cb));
        theDialog->AddInitializeHandler(make_callback(this, &secondUI::initialize_cb));
        theDialog->AddDialogShownHandler(make_callback(this, &secondUI::dialogShown_cb));
        m_opMode = 0;
    }
    catch(exception&) { throw; }
}

secondUI::~secondUI()
{
    if (theDialog) { delete theDialog; theDialog = NULL; }
}

extern "C" DllExport void ufusr(char *param, int *retcod, int param_len)
{
    secondUI *dlg = NULL;
    try
    {
        dlg = new secondUI();
        dlg->Launch();
    }
    catch(exception& ex)
    {
        secondUI::theUI->NXMessageBox()->Show("secondUI",
            NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    if (dlg) { delete dlg; dlg = NULL; }
}

extern "C" DllExport int ufusr_ask_unload() { return (int)Session::LibraryUnloadOptionImmediately; }
extern "C" DllExport void ufusr_cleanup(void) {}

NXOpen::BlockStyler::BlockDialog::DialogResponse secondUI::Launch()
{
    auto r = NXOpen::BlockStyler::BlockDialog::DialogResponseInvalid;
    try { r = theDialog->Launch(); }
    catch(exception& ex) { secondUI::theUI->NXMessageBox()->Show("Block Styler",
        NXOpen::NXMessageBox::DialogTypeError, ex.what()); }
    return r;
}

void secondUI::initialize_cb()
{
    try
    {
        group3 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group3"));
        button0 = dynamic_cast<NXOpen::BlockStyler::Button*>(theDialog->TopBlock()->FindBlock("button0"));
        button01 = dynamic_cast<NXOpen::BlockStyler::Button*>(theDialog->TopBlock()->FindBlock("button01"));
        group2 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group2"));
        selection0 = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(theDialog->TopBlock()->FindBlock("selection0"));
        group1 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group1"));
        point0 = dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(theDialog->TopBlock()->FindBlock("point0"));
        enum0 = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(theDialog->TopBlock()->FindBlock("enum0"));
        group = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group"));
        point01 = dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(theDialog->TopBlock()->FindBlock("point01"));
        enum01 = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(theDialog->TopBlock()->FindBlock("enum01"));
    }
    catch(exception& ex) { secondUI::theUI->NXMessageBox()->Show("Block Styler",
        NXOpen::NXMessageBox::DialogTypeError, ex.what()); }
}

void secondUI::dialogShown_cb() {}

//----------------------------------------------------------------------
// 核心执行（纯 NXOpen API）
//----------------------------------------------------------------------
int secondUI::apply_cb()
{
    int err = 0;
    try
    {
        Body *body = GetBody(selection0);
        if (!body) throw std::runtime_error("Select a body.");
        Point3d target = point01->Point();
        Part *wp = theSession->Parts()->Work();
        if (!wp) throw std::runtime_error("No work part.");
        Session::UndoMarkId mark = theSession->SetUndoMark(
            Session::MarkVisibilityVisible, m_opMode==0?"Move":"Copy");
        try
        {
            if (m_opMode == 0) theSession->UpdateManager()->AddToDeleteList(body);
            auto bb = wp->Features()->CreateBlockFeatureBuilder(nullptr);
            bb->SetOriginAndLengths(Point3d(target.X-50,target.Y-50,target.Z-50),
                "100","100","100");
            bb->CommitFeature();
            bb->Destroy();
            theSession->UpdateManager()->DoUpdate(mark);
        }
        catch (...)
        {
            theSession->UndoToMark(mark, NULL);
            throw;
        }
    }
    catch (std::exception& ex)
    {
        err = 1;
        theUI->NXMessageBox()->Show("secondUI",
            NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    return err;
}
//----------------------------------------------------------------------
int secondUI::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    try
    {
        if (block == button0) { m_opMode = 0; }
        else if (block == button01) { m_opMode = 1; }
    }
    catch(exception& ex) { secondUI::theUI->NXMessageBox()->Show("Block Styler",
        NXOpen::NXMessageBox::DialogTypeError, ex.what()); }
    return 0;
}

int secondUI::ok_cb()
{
    int ec = 0;
    try { ec = apply_cb(); }
    catch (exception& ex) { ec = 1; }
    return ec;
}

PropertyList* secondUI::GetBlockProperties(const char *id) { return theDialog->GetBlockProperties(id); }
