#include "secondUI.hpp"
#include <set>
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

static tag_t ResolveBodyTag(TaggedObject *obj)
{
    if (!obj) return NULL_TAG;
    if (auto b = dynamic_cast<Body*>(obj)) return b->Tag();
    if (auto f = dynamic_cast<Face*>(obj)) { auto b = f->GetBody(); return b ? b->Tag() : f->Tag(); }
    if (auto e = dynamic_cast<Edge*>(obj)) { auto b = e->GetBody(); return b ? b->Tag() : e->Tag(); }
    return obj->Tag();
}

static std::vector<tag_t> GetBodyTags(NXOpen::BlockStyler::SelectObject *sel)
{
    std::vector<tag_t> tags;
    if (!sel) return tags;
    std::set<tag_t> seen;
    for (auto obj : sel->GetSelectedObjects())
    {
        tag_t t = ResolveBodyTag(obj);
        if (t != NULL_TAG && seen.insert(t).second) tags.push_back(t);
    }
    return tags;
}

static Point3d BoxCenter(tag_t tag)
{
    double box[6] = {0};
    int err = UF_MODL_ask_bounding_box(tag, box);
    if (err) throw std::runtime_error("Bounding box failed");
    return Point3d((box[0]+box[3])/2, (box[1]+box[4])/2, (box[2]+box[5])/2);
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
    UF_initialize();
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
    UF_terminate();
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
// 核心执行（UF_MODL_transform_entities + uf5947）
//----------------------------------------------------------------------
int secondUI::apply_cb()
{
    int err = 0;
    try
    {
        auto tags = GetBodyTags(selection0);
        if (tags.empty()) throw std::runtime_error("Select a body to move/copy.");

        Point3d center = BoxCenter(tags[0]);
        Point3d p0 = point0->Point();
        Point3d target = point01->Point();
        { char b[256]; snprintf(b,256,"point0=(%.1f,%.1f,%.1f) point01=(%.1f,%.1f,%.1f) center=(%.1f,%.1f,%.1f)",p0.X,p0.Y,p0.Z,target.X,target.Y,target.Z,center.X,center.Y,center.Z); Log(b); }

        double dx = target.X - center.X;
        double dy = target.Y - center.Y;
        double dz = target.Z - center.Z;
        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 1e-6) return 0;  // Already moved, nothing to do

        Part *wp = theSession->Parts()->Work();
        if (!wp) throw std::runtime_error("No work part.");

        int n = (int)tags.size();
        if (m_opMode == 0) // Move
        {
            double matrix[16] = {1,0,0,dx, 0,1,0,dy, 0,0,1,dz, 0,0,0,1};
            int st = UF_MODL_transform_entities(n, tags.data(), matrix);
            if (st) { std::ostringstream ss; ss<<"Move err "<<st; throw std::runtime_error(ss.str()); }
            Log("Move OK");
        }
        else // Copy
        {
            double delta[3] = {dx, dy, dz};
            double m12[12] = {0};
            uf5943(delta, m12);
            int mc=2, layer=0, trace=2, status=0;
            tag_t traceGrp=NULL_TAG;
            std::vector<tag_t> copies(n, NULL_TAG);
            uf5947(m12, tags.data(), &n, &mc, &layer, &trace, copies.data(), &traceGrp, &status);
            if (status) { std::ostringstream ss; ss<<"Copy err "<<status; throw std::runtime_error(ss.str()); }
            Log("Copy OK");
        }
    }
    catch (std::exception& ex) {
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
        if (block == button0) { m_opMode = 0; apply_cb(); }
        else if (block == button01) { m_opMode = 1; apply_cb(); }
    }
    catch(exception& ex) { secondUI::theUI->NXMessageBox()->Show("Block Styler",
        NXOpen::NXMessageBox::DialogTypeError, ex.what()); }
    return 0;
}

int secondUI::ok_cb() { return 0; }

PropertyList* secondUI::GetBlockProperties(const char *id) { return theDialog->GetBlockProperties(id); }
