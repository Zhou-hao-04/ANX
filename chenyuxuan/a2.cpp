#pragma warning(disable:4530)
#pragma execution_character_set("utf-8")

#include <uf.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_trns.h>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/WCS.hxx>
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/BlockStyler_SpecifyPoint.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/ugmath.hxx>
#include <windows.h>

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace NXOpen;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
enum class OperationMode
{
    None,
    Move,
    Copy
};

enum class StartPointMode
{
    BodyCenterTop = 0,
    BodyCenter = 1,
    BodyCenterBottom = 2,
    FaceCenter = 3
};

enum class FinishPointMode
{
    AbsOrigin = 0,
    WcsOrigin = 1,
    BodyCenter = 2,
    FaceCenter = 3,
    XZero = 4,
    YZero = 5,
    ZZero = 6,
    SpecifiedPoint = 7
};

Session* g_session = nullptr;
UI* g_ui = nullptr;
Part* g_workPart = nullptr;

std::string getDllDirectory()
{
    char buffer[MAX_PATH] = {0};
    HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    DWORD length = GetModuleFileNameA(module, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return ".";
    }

    std::string path(buffer);
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

std::string getDlxPath()
{
    return getDllDirectory() + "\\a2.dlx";
}

void showError(const std::string& message)
{
    if (g_ui)
    {
        g_ui->NXMessageBox()->Show("错误", NXMessageBox::DialogTypeError, message.c_str());
    }
}

double vectorLength(const Vector3d& vector)
{
    return std::sqrt(vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);
}

std::string statusText(int status)
{
    switch (status)
    {
    case 0: return "成功";
    case 1: return "对象数量无效";
    case 2: return "变换参数无效";
    case 3: return "对象无效或不可变换";
    case 4: return "对象不支持当前矩阵";
    case 5: return "建模更新失败";
    case 6: return "复制实体添加建模数据失败";
    case 7: return "不能缩放参数化实体";
    case 8: return "变换矩阵不适用于对象";
    case 9: return "不能缩放展开曲线";
    case 10: return "不能镜像实体";
    case 11: return "不能变换装配 occurrence";
    case 12: return "不能变换草图或草图曲线";
    default: return "未知错误";
    }
}

Point3d askBoxCenter(tag_t objectTag)
{
    double box[6] = {0.0};
    int errorCode = UF_MODL_ask_bounding_box(objectTag, box);
    if (errorCode != 0)
    {
        std::ostringstream ss;
        ss << "读取对象包围盒失败，错误码 " << errorCode;
        throw std::runtime_error(ss.str());
    }
    return Point3d((box[0] + box[3]) * 0.5, (box[1] + box[4]) * 0.5, (box[2] + box[5]) * 0.5);
}

Point3d askBodyPoint(tag_t bodyTag, StartPointMode mode)
{
    double box[6] = {0.0};
    int errorCode = UF_MODL_ask_bounding_box(bodyTag, box);
    if (errorCode != 0)
    {
        std::ostringstream ss;
        ss << "读取体包围盒失败，错误码 " << errorCode;
        throw std::runtime_error(ss.str());
    }

    double x = (box[0] + box[3]) * 0.5;
    double y = (box[1] + box[4]) * 0.5;
    if (mode == StartPointMode::BodyCenterTop)
    {
        return Point3d(x, y, box[5]);
    }
    if (mode == StartPointMode::BodyCenterBottom)
    {
        return Point3d(x, y, box[2]);
    }
    return Point3d(x, y, (box[2] + box[5]) * 0.5);
}

tag_t resolveBodyTag(TaggedObject* object)
{
    if (!object)
    {
        return NULL_TAG;
    }

    if (Body* body = dynamic_cast<Body*>(object))
    {
        return body->Tag();
    }
    if (Face* face = dynamic_cast<Face*>(object))
    {
        Body* body = face->GetBody();
        return body ? body->Tag() : face->Tag();
    }
    if (Edge* edge = dynamic_cast<Edge*>(object))
    {
        Body* body = edge->GetBody();
        return body ? body->Tag() : edge->Tag();
    }
    return object->Tag();
}

tag_t resolveReferenceTag(TaggedObject* object)
{
    return object ? object->Tag() : NULL_TAG;
}

std::vector<NXOpen::Body*> collectTransformBodies(BlockStyler::SelectObject* selectObject)
{
    std::vector<NXOpen::Body*> bodies;
    if (!selectObject)
    {
        return bodies;
    }

    std::set<tag_t> uniqueTags;
    std::vector<TaggedObject*> selectedObjects = selectObject->GetSelectedObjects();
    for (TaggedObject* object : selectedObjects)
    {
        tag_t tag = resolveBodyTag(object);
        if (tag != NULL_TAG && uniqueTags.insert(tag).second)
        {
            Body* body = dynamic_cast<Body*>(object);
            if (body)
            {
                bodies.push_back(body);
            }
        }
    }
    return bodies;
}

TaggedObject* readPrimaryObject(BlockStyler::SelectObject* selectObject)
{
    if (!selectObject)
    {
        return nullptr;
    }
    std::vector<TaggedObject*> selectedObjects = selectObject->GetSelectedObjects();
    return selectedObjects.empty() ? nullptr : selectedObjects.front();
}

int getEnumValue(BlockStyler::Enumeration* enumeration)
{
    if (!enumeration)
    {
        return 0;
    }
    BlockStyler::PropertyList* properties = enumeration->GetProperties();
    return properties ? properties->GetEnum("Value") : 0;
}

void setPointOptional(BlockStyler::SpecifyPoint* pointBlock)
{
    if (!pointBlock)
    {
        return;
    }

    try
    {
        pointBlock->SetStepStatusAsString("Optional");
    }
    catch (...)
    {
        // 旧 DLX 或 NX 版本不支持动态修改 StepStatus 时，继续让 DLX 自身属性生效。
    }
}

Point3d resolveStartPoint(StartPointMode mode, TaggedObject* primaryObject, BlockStyler::SpecifyPoint* manualPoint)
{
    if (!primaryObject)
    {
        throw std::runtime_error("请选择操作对象");
    }

    if (mode == StartPointMode::FaceCenter)
    {
        if (Face* face = dynamic_cast<Face*>(primaryObject))
        {
            return askBoxCenter(face->Tag());
        }
        if (manualPoint)
        {
            return manualPoint->Point();
        }
        throw std::runtime_error("起始点为面中心时，请选择面或指定点");
    }

    tag_t bodyTag = resolveBodyTag(primaryObject);
    if (bodyTag == NULL_TAG)
    {
        throw std::runtime_error("无法从选择对象解析体中心");
    }
    return askBodyPoint(bodyTag, mode);
}

Point3d resolveFinishPoint(FinishPointMode mode, TaggedObject* primaryObject, const Point3d& startPoint, BlockStyler::SpecifyPoint* manualPoint)
{
    switch (mode)
    {
    case FinishPointMode::AbsOrigin:
        return Point3d(0.0, 0.0, 0.0);
    case FinishPointMode::WcsOrigin:
        if (g_workPart && g_workPart->WCS())
        {
            return g_workPart->WCS()->Origin();
        }
        return Point3d(0.0, 0.0, 0.0);
    case FinishPointMode::BodyCenter:
        if (!primaryObject)
        {
            throw std::runtime_error("结束点为体中心时，请选择操作对象");
        }
        return askBoxCenter(resolveBodyTag(primaryObject));
    case FinishPointMode::FaceCenter:
        if (Face* face = dynamic_cast<Face*>(primaryObject))
        {
            return askBoxCenter(face->Tag());
        }
        if (manualPoint)
        {
            return manualPoint->Point();
        }
        throw std::runtime_error("结束点为面中心时，请选择面或指定点");
    case FinishPointMode::XZero:
        return Point3d(0.0, startPoint.Y, startPoint.Z);
    case FinishPointMode::YZero:
        return Point3d(startPoint.X, 0.0, startPoint.Z);
    case FinishPointMode::ZZero:
        return Point3d(startPoint.X, startPoint.Y, 0.0);
    case FinishPointMode::SpecifiedPoint:
        if (manualPoint)
        {
            return manualPoint->Point();
        }
        throw std::runtime_error("结束点为指定点时，请指定点");
    default:
        throw std::runtime_error("未知结束点枚举值");
    }
}

void executeMoveOrCopy(const std::vector<NXOpen::Body*>& bodies, const Point3d& startPt, const Point3d& endPt, OperationMode operationMode)
{
    if (bodies.empty())
    {
        throw std::runtime_error("请选择操作对象");
    }
    if (vectorLength(Vector3d(endPt.X - startPt.X, endPt.Y - startPt.Y, endPt.Z - startPt.Z)) < 1.0e-9)
    {
        throw std::runtime_error("起始点和结束点重合，不能执行零向量移动/复制");
    }

    Features::MoveObjectBuilder* moveBuilder =
        g_workPart->BaseFeatures()->CreateMoveObjectBuilder(nullptr);

    try
    {
        if (operationMode == OperationMode::Copy)
        {
            moveBuilder->SetMoveObjectResult(
                Features::MoveObjectBuilder::MoveObjectResultOptionsCopyOriginal);
        }
        else
        {
            moveBuilder->SetMoveObjectResult(
                Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);
        }

        for (auto* body : bodies)
        {
            moveBuilder->ObjectToMoveObject()->Add(body);
        }

        GeometricUtilities::ModlMotion* motion = moveBuilder->TransformMotion();
        motion->SetOption(GeometricUtilities::ModlMotion::OptionsDeltaXyz);
        motion->SetDeltaEnum(GeometricUtilities::ModlMotion::DeltaReferenceAcsWorkPart);
        motion->DeltaXc()->SetValue(endPt.X - startPt.X);
        motion->DeltaYc()->SetValue(endPt.Y - startPt.Y);
        motion->DeltaZc()->SetValue(endPt.Z - startPt.Z);

        moveBuilder->Commit();
        moveBuilder->Destroy();
        moveBuilder = nullptr;
    }
    catch (const NXException&)
    {
        if (moveBuilder) moveBuilder->Destroy();
        throw;
    }
}

class DotMoveDialog
{
public:
    DotMoveDialog()
    {
        m_dialog = g_ui->CreateDialog(getDlxPath().c_str());
        m_dialog->AddApplyHandler(NXOpen::make_callback(this, &DotMoveDialog::apply_cb));
        m_dialog->AddOkHandler(NXOpen::make_callback(this, &DotMoveDialog::ok_cb));
        m_dialog->AddUpdateHandler(NXOpen::make_callback(this, &DotMoveDialog::update_cb));
        m_dialog->AddInitializeHandler(NXOpen::make_callback(this, &DotMoveDialog::initialize_cb));
    }

    void show()
    {
        m_dialog->Launch();
    }

    void initialize_cb()
    {
        m_moveButton = dynamic_cast<BlockStyler::Button*>(m_dialog->TopBlock()->FindBlock("button0"));
        m_copyButton = dynamic_cast<BlockStyler::Button*>(m_dialog->TopBlock()->FindBlock("button01"));
        m_selectObject = dynamic_cast<BlockStyler::SelectObject*>(m_dialog->TopBlock()->FindBlock("selection0"));
        m_startPoint = dynamic_cast<BlockStyler::SpecifyPoint*>(m_dialog->TopBlock()->FindBlock("point0"));
        m_startPointEnum = dynamic_cast<BlockStyler::Enumeration*>(m_dialog->TopBlock()->FindBlock("enum0"));
        m_finishPoint = dynamic_cast<BlockStyler::SpecifyPoint*>(m_dialog->TopBlock()->FindBlock("point01"));
        m_finishPointEnum = dynamic_cast<BlockStyler::Enumeration*>(m_dialog->TopBlock()->FindBlock("enum01"));

        m_toggle = dynamic_cast<BlockStyler::Toggle*>(m_dialog->TopBlock()->FindBlock("toggle0"));
        updateStartPointUI();

        setPointOptional(m_startPoint);
        setPointOptional(m_finishPoint);
    }

    int update_cb(BlockStyler::UIBlock* block)
    {
        if (block == m_moveButton)
        {
            m_operationMode = OperationMode::Move;
        }
        else if (block == m_copyButton)
        {
            m_operationMode = OperationMode::Copy;
        }
        else if (block == m_selectObject)
        {
            updateStartPointUI();
        }
        else if (block == m_startPoint)
        {
            m_startPtSpecified = true;
        }
        else if (block == m_finishPoint)
        {
            m_endPtSpecified = true;
        }
        else if (block == m_toggle)
        {
            updateStartPointUI();
        }
        return 0;
    }

    int apply_cb()
    {
        return executeFromDialog() ? 0 : 1;
    }

    int ok_cb()
    {
        return executeFromDialog() ? 0 : 1;
    }

private:
    BlockStyler::BlockDialog* m_dialog = nullptr;
    BlockStyler::Button* m_moveButton = nullptr;
    BlockStyler::Button* m_copyButton = nullptr;
    BlockStyler::SelectObject* m_selectObject = nullptr;
    BlockStyler::SpecifyPoint* m_startPoint = nullptr;
    BlockStyler::Enumeration* m_startPointEnum = nullptr;
    BlockStyler::SpecifyPoint* m_finishPoint = nullptr;
    BlockStyler::Enumeration* m_finishPointEnum = nullptr;
    bool m_startPtSpecified = false;
    bool m_endPtSpecified = false;
    BlockStyler::Toggle* m_toggle = nullptr;
        OperationMode m_operationMode = OperationMode::None;

    void updateStartPointUI()
    {
        if (!m_selectObject || !m_startPointEnum || !m_startPoint || !m_toggle)
        {
            return;
        }

        bool multiEnabled = m_toggle->Value();

        try
        {
            BlockStyler::PropertyList* props = m_selectObject->GetProperties();
            props->SetEnum("SelectMode", multiEnabled ? 1 : 0);
        }
        catch (...)
        {
        }

        if (multiEnabled)
        {
            int count = static_cast<int>(m_selectObject->GetSelectedObjects().size());
            if (count >= 2)
            {
                m_startPointEnum->SetEnable(false);
                m_startPtSpecified = true;
            }
            else
            {
                m_startPointEnum->SetEnable(true);
            }
        }
        else
        {
            m_startPointEnum->SetEnable(true);
            m_startPtSpecified = false;
        }
    }

        bool executeFromDialog()
    {
        Session::UndoMarkId undoMark = g_session->SetUndoMark(Session::MarkVisibilityVisible, "点到点移动与复制");
        try
        {
            if (m_operationMode == OperationMode::None)
            {
                throw std::runtime_error("请先选择移动或复制模式");
            }

            TaggedObject* primaryObject = readPrimaryObject(m_selectObject);
            std::vector<NXOpen::Body*> selectedBodies = collectTransformBodies(m_selectObject);
            if (!primaryObject || selectedBodies.empty())
            {
                throw std::runtime_error("请选择操作对象");
            }

            // Resolve start point - manual point overrides enum mode
            Point3d start;
            if (m_startPtSpecified && m_startPoint)
            {
                start = m_startPoint->Point();
            }
            else
            {
                StartPointMode startMode = static_cast<StartPointMode>(getEnumValue(m_startPointEnum));
                start = resolveStartPoint(startMode, primaryObject, m_startPoint);
            }

            // Resolve end point - manual point overrides enum mode
            Point3d finish;
            if (m_endPtSpecified && m_finishPoint)
            {
                finish = m_finishPoint->Point();
            }
            else
            {
                FinishPointMode finishMode = static_cast<FinishPointMode>(getEnumValue(m_finishPointEnum));
                finish = resolveFinishPoint(finishMode, primaryObject, start, m_finishPoint);
            }

            executeMoveOrCopy(selectedBodies, start, finish, m_operationMode);
            if (g_session && g_session->UpdateManager())
            {
                g_session->UpdateManager()->DoUpdate(undoMark);
            }
            return true;
        }
        catch (const NXException& ex)
        {
            g_session->UndoToMark(undoMark, "点到点移动与复制");
            showError(ex.Message());
        }
        catch (const std::exception& ex)
        {
            g_session->UndoToMark(undoMark, "点到点移动与复制");
            showError(ex.what());
        }
        return false;
    }
};
}

extern "C" DllExport void ufusr(char* param, int* retCode, int paramLen)
{
    UF_initialize();
    g_session = Session::GetSession();
    g_ui = UI::GetUI();
    g_workPart = g_session->Parts()->Work();

    try
    {
        DotMoveDialog dialog;
        dialog.show();
        *retCode = 0;
    }
    catch (const NXException& ex)
    {
        showError(ex.Message());
        *retCode = 1;
    }
    catch (const std::exception& ex)
    {
        showError(ex.what());
        *retCode = 1;
    }

    UF_terminate();
}

