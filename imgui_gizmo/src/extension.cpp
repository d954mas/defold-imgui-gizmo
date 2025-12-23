#define EXTENSION_NAME ImguiGizmo
#define LIB_NAME "ImguiGizmo"
#define MODULE_NAME "imgui_gizmo"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <vector>

#include <dmsdk/sdk.h>

#include "imgui.h"
#include "imguizmo.h"


static void Matrix4ToFloatArray(const dmVMath::Matrix4& matrix, float* out_array)
{
    dmVMath::Vector4 col0 = matrix.getCol0();
    dmVMath::Vector4 col1 = matrix.getCol1();
    dmVMath::Vector4 col2 = matrix.getCol2();
    dmVMath::Vector4 col3 = matrix.getCol3();

    out_array[0] = col0.getX();
    out_array[1] = col0.getY();
    out_array[2] = col0.getZ();
    out_array[3] = col0.getW();

    out_array[4] = col1.getX();
    out_array[5] = col1.getY();
    out_array[6] = col1.getZ();
    out_array[7] = col1.getW();

    out_array[8] = col2.getX();
    out_array[9] = col2.getY();
    out_array[10] = col2.getZ();
    out_array[11] = col2.getW();

    out_array[12] = col3.getX();
    out_array[13] = col3.getY();
    out_array[14] = col3.getZ();
    out_array[15] = col3.getW();
}

static dmVMath::Quat EulerToQuat(dmVMath::Vector3 xyz)
{
    const float half_rad_factor = 0.5f * 3.14159265358979323846f / 180.0f;
    uint8_t mask = (xyz.getZ() != 0.0f) << 2 | (xyz.getY() != 0.0f) << 1 | (xyz.getX() != 0.0f);
    switch (mask) {
    case 0:
        return dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    case 1:
    case 2:
    case 4: {
        float ha = (xyz.getX() + xyz.getY() + xyz.getZ()) * half_rad_factor;
        dmVMath::Quat q(0.0f, 0.0f, 0.0f, cosf(ha));
        q.setElem(mask >> 1, sinf(ha));
        return q;
    }
    default:
        break;
    }

    float t1 = xyz.getY() * half_rad_factor;
    float t2 = xyz.getZ() * half_rad_factor;
    float t3 = xyz.getX() * half_rad_factor;

    float c1 = cosf(t1);
    float s1 = sinf(t1);
    float c2 = cosf(t2);
    float s2 = sinf(t2);
    float c3 = cosf(t3);
    float s3 = sinf(t3);
    float c1_c2 = c1 * c2;
    float s2_s3 = s2 * s3;

    dmVMath::Quat quat;
    quat.setW(-s1 * s2_s3 + c1_c2 * c3);
    quat.setX(s1 * s2 * c3 + s3 * c1_c2);
    quat.setY(s1 * c2 * c3 + s2_s3 * c1);
    quat.setZ(-s1 * s3 * c2 + s2 * c1 * c3);
    return quat;
}

static void FloatArrayToMatrix4(const float* array, dmVMath::Matrix4* out_matrix)
{
    *out_matrix = dmVMath::Matrix4(
        dmVMath::Vector4(array[0], array[1], array[2], array[3]),
        dmVMath::Vector4(array[4], array[5], array[6], array[7]),
        dmVMath::Vector4(array[8], array[9], array[10], array[11]),
        dmVMath::Vector4(array[12], array[13], array[14], array[15])
    );
}

static bool ReadVector3(lua_State* L, int index, float out_vec[3])
{
    if (!dmScript::IsVector3(L, index)) {
        return false;
    }
    dmVMath::Vector3* v3 = dmScript::CheckVector3(L, index);
    out_vec[0] = v3->getX();
    out_vec[1] = v3->getY();
    out_vec[2] = v3->getZ();
    return true;
}

static bool ReadVector3OrNumber(lua_State* L, int index, float out_vec[3])
{
    if (lua_isnumber(L, index)) {
        float v = (float)lua_tonumber(L, index);
        out_vec[0] = v;
        out_vec[1] = v;
        out_vec[2] = v;
        return true;
    }
    return ReadVector3(L, index, out_vec);
}

static bool ReadBounds(lua_State* L, int index, float out_bounds[6])
{
    if (!lua_istable(L, index)) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        lua_rawgeti(L, index, i + 1);
        out_bounds[i] = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    return true;
}

static bool ReadColor(lua_State* L, int index, ImVec4* out_color)
{
    if (dmScript::IsVector4(L, index)) {
        dmVMath::Vector4* v4 = dmScript::CheckVector4(L, index);
        out_color->x = v4->getX();
        out_color->y = v4->getY();
        out_color->z = v4->getZ();
        out_color->w = v4->getW();
        return true;
    }
    if (lua_istable(L, index)) {
        float values[4];
        for (int i = 0; i < 4; ++i) {
            lua_rawgeti(L, index, i + 1);
            values[i] = (float)luaL_checknumber(L, -1);
            lua_pop(L, 1);
        }
        *out_color = ImVec4(values[0], values[1], values[2], values[3]);
        return true;
    }
    return false;
}

static int gizmo_SetRect(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float width = (float)luaL_checknumber(L, 3);
    float height = (float)luaL_checknumber(L, 4);
    ImGuizmo::SetRect(x, y, width, height);
    return 0;
}

static int gizmo_SetOrthographic(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    bool value = lua_toboolean(L, 1) != 0;
    ImGuizmo::SetOrthographic(value);
    return 0;
}

static int gizmo_Enable(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    bool value = lua_toboolean(L, 1) != 0;
    ImGuizmo::Enable(value);
    return 0;
}

static int gizmo_IsOver(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    lua_pushboolean(L, ImGuizmo::IsOver());
    return 1;
}

static int gizmo_IsOverOperation(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    ImGuizmo::OPERATION op = (ImGuizmo::OPERATION)luaL_checkinteger(L, 1);
    lua_pushboolean(L, ImGuizmo::IsOver(op));
    return 1;
}

static int gizmo_IsOverPosition(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    float position[3];
    if (!ReadVector3(L, 1, position)) {
        return DM_LUA_ERROR("position must be vmath.vector3");
    }
    float radius = (float)luaL_checknumber(L, 2);
    lua_pushboolean(L, ImGuizmo::IsOver(position, radius));
    return 1;
}

static int gizmo_IsUsing(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    lua_pushboolean(L, ImGuizmo::IsUsing());
    return 1;
}

static int gizmo_IsUsingViewManipulate(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    lua_pushboolean(L, ImGuizmo::IsUsingViewManipulate());
    return 1;
}

static int gizmo_IsUsingAny(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    lua_pushboolean(L, ImGuizmo::IsUsingAny());
    return 1;
}

static int gizmo_SetGizmoSizeClipSpace(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    float value = (float)luaL_checknumber(L, 1);
    ImGuizmo::SetGizmoSizeClipSpace(value);
    return 0;
}

static int gizmo_AllowAxisFlip(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    bool value = lua_toboolean(L, 1) != 0;
    ImGuizmo::AllowAxisFlip(value);
    return 0;
}

static int gizmo_SetAxisLimit(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    float value = (float)luaL_checknumber(L, 1);
    ImGuizmo::SetAxisLimit(value);
    return 0;
}

static int gizmo_SetAxisMask(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    bool x = lua_toboolean(L, 1) != 0;
    bool y = lua_toboolean(L, 2) != 0;
    bool z = lua_toboolean(L, 3) != 0;
    ImGuizmo::SetAxisMask(x, y, z);
    return 0;
}

static int gizmo_SetPlaneLimit(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    float value = (float)luaL_checknumber(L, 1);
    ImGuizmo::SetPlaneLimit(value);
    return 0;
}

static int gizmo_Manipulate(lua_State* L)
{
    int top = lua_gettop(L);
    if (top < 5) {
        return luaL_error(L, "manipulate(view, projection, operation, mode, matrix, [snap], [local_bounds], [bounds_snap])");
    }
    ImGuizmo::BeginFrame();
    dmVMath::Matrix4 view = *dmScript::CheckMatrix4(L, 1);
    dmVMath::Matrix4 projection = *dmScript::CheckMatrix4(L, 2);
    ImGuizmo::OPERATION operation = (ImGuizmo::OPERATION)luaL_checkinteger(L, 3);
    ImGuizmo::MODE mode = (ImGuizmo::MODE)luaL_checkinteger(L, 4);
    dmVMath::Matrix4* gizmo_matrix = dmScript::CheckMatrix4(L, 5);

    float view_matrix[16];
    float projection_matrix[16];
    float model_matrix[16];
    float delta_matrix[16];
    Matrix4ToFloatArray(view, view_matrix);
    Matrix4ToFloatArray(projection, projection_matrix);
    Matrix4ToFloatArray(*gizmo_matrix, model_matrix);

    const float* snap = NULL;
    float snap_values[3];
    if (top >= 6 && !lua_isnil(L, 6)) {
        if (!ReadVector3OrNumber(L, 6, snap_values)) {
            return luaL_error(L, "snap must be number or vmath.vector3");
        }
        snap = snap_values;
    }

    const float* local_bounds = NULL;
    float local_bounds_values[6];
    if (top >= 7 && !lua_isnil(L, 7)) {
        if (!ReadBounds(L, 7, local_bounds_values)) {
            return luaL_error(L, "local_bounds must be table with 6 numbers");
        }
        local_bounds = local_bounds_values;
    }

    const float* bounds_snap = NULL;
    float bounds_snap_values[3];
    if (top >= 8 && !lua_isnil(L, 8)) {
        if (!ReadVector3OrNumber(L, 8, bounds_snap_values)) {
            return luaL_error(L, "bounds_snap must be number or vmath.vector3");
        }
        bounds_snap = bounds_snap_values;
    }

    bool manipulated = ImGuizmo::Manipulate(
        view_matrix,
        projection_matrix,
        operation,
        mode,
        model_matrix,
        delta_matrix,
        snap,
        local_bounds,
        bounds_snap
    );

    FloatArrayToMatrix4(model_matrix, gizmo_matrix);

    lua_pushboolean(L, manipulated);
    if (manipulated) {
        dmVMath::Matrix4 delta;
        FloatArrayToMatrix4(delta_matrix, &delta);
        dmScript::PushMatrix4(L, delta);
        return 2;
    }
    return 1;
}

static int gizmo_DecomposeMatrix(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 3);
    dmVMath::Matrix4 matrix = *dmScript::CheckMatrix4(L, 1);
    float m[16];
    Matrix4ToFloatArray(matrix, m);

    float translation[3];
    float rotation[3];
    float scale[3];

    ImGuizmo::DecomposeMatrixToComponents(m, translation, rotation, scale);

    dmScript::PushVector3(L, dmVMath::Vector3(translation[0], translation[1], translation[2]));
    dmScript::PushVector3(L, dmVMath::Vector3(rotation[0], rotation[1], rotation[2]));
    dmScript::PushVector3(L, dmVMath::Vector3(scale[0], scale[1], scale[2]));
    return 3;
}

static int gizmo_RecomposeMatrix(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    float translation[3];
    float rotation[3];
    float scale[3];

    if (!ReadVector3(L, 1, translation)) {
        return DM_LUA_ERROR("translation must be vmath.vector3");
    }
    if (!ReadVector3(L, 2, rotation)) {
        return DM_LUA_ERROR("rotation must be vmath.vector3");
    }
    if (!ReadVector3(L, 3, scale)) {
        return DM_LUA_ERROR("scale must be vmath.vector3");
    }

    float m[16];
    ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, m);

    dmVMath::Matrix4 matrix;
    FloatArrayToMatrix4(m, &matrix);
    dmScript::PushMatrix4(L, matrix);
    return 1;
}

static int gizmo_DrawGrid(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    dmVMath::Matrix4 view = *dmScript::CheckMatrix4(L, 1);
    dmVMath::Matrix4 projection = *dmScript::CheckMatrix4(L, 2);
    dmVMath::Matrix4 grid_matrix = *dmScript::CheckMatrix4(L, 3);
    float grid_size = (float)luaL_checknumber(L, 4);

    float view_matrix[16];
    float projection_matrix[16];
    float matrix[16];
    Matrix4ToFloatArray(view, view_matrix);
    Matrix4ToFloatArray(projection, projection_matrix);
    Matrix4ToFloatArray(grid_matrix, matrix);

    ImGuizmo::DrawGrid(view_matrix, projection_matrix, matrix, grid_size);
    return 0;
}

static int gizmo_DrawCubes(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    dmVMath::Matrix4 view = *dmScript::CheckMatrix4(L, 1);
    dmVMath::Matrix4 projection = *dmScript::CheckMatrix4(L, 2);

    if (!lua_istable(L, 3)) {
        return DM_LUA_ERROR("matrices must be a table of vmath.matrix4");
    }

    int count = (int)lua_objlen(L, 3);
    if (count == 0) {
        return 0;
    }

    std::vector<float> matrices;
    matrices.resize((size_t)count * 16);

    for (int i = 0; i < count; ++i) {
        lua_rawgeti(L, 3, i + 1);
        dmVMath::Matrix4* m = dmScript::CheckMatrix4(L, -1);
        Matrix4ToFloatArray(*m, &matrices[(size_t)i * 16]);
        lua_pop(L, 1);
    }

    float view_matrix[16];
    float projection_matrix[16];
    Matrix4ToFloatArray(view, view_matrix);
    Matrix4ToFloatArray(projection, projection_matrix);

    ImGuizmo::DrawCubes(view_matrix, projection_matrix, matrices.data(), count);
    return 0;
}

static int gizmo_ViewManipulate(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    int top = lua_gettop(L);
    if (top < 5) {
        return DM_LUA_ERROR("view_manipulate(view, length, position, size, background_color) or view_manipulate(view, projection, operation, mode, matrix, length, position, size, background_color)");
    }
    ImGuizmo::BeginFrame();

    dmVMath::Matrix4* view = dmScript::CheckMatrix4(L, 1);
    float view_matrix[16];
    Matrix4ToFloatArray(*view, view_matrix);

    if (dmScript::IsMatrix4(L, 2)) {
        dmVMath::Matrix4 projection = *dmScript::CheckMatrix4(L, 2);
        ImGuizmo::OPERATION operation = (ImGuizmo::OPERATION)luaL_checkinteger(L, 3);
        ImGuizmo::MODE mode = (ImGuizmo::MODE)luaL_checkinteger(L, 4);
        dmVMath::Matrix4* matrix = dmScript::CheckMatrix4(L, 5);
        float length = (float)luaL_checknumber(L, 6);

        float position_vec[3];
        float size_vec[3];
        if (!ReadVector3(L, 7, position_vec)) {
            return DM_LUA_ERROR("position must be vmath.vector3");
        }
        if (!ReadVector3(L, 8, size_vec)) {
            return DM_LUA_ERROR("size must be vmath.vector3");
        }
        ImU32 background_color = (ImU32)luaL_checkinteger(L, 9);

        float projection_matrix[16];
        float model_matrix[16];
        Matrix4ToFloatArray(projection, projection_matrix);
        Matrix4ToFloatArray(*matrix, model_matrix);

        ImGuizmo::ViewManipulate(
            view_matrix,
            projection_matrix,
            operation,
            mode,
            model_matrix,
            length,
            ImVec2(position_vec[0], position_vec[1]),
            ImVec2(size_vec[0], size_vec[1]),
            background_color
        );

        FloatArrayToMatrix4(model_matrix, matrix);
    } else {
        float length = (float)luaL_checknumber(L, 2);
        float position_vec[3];
        float size_vec[3];
        if (!ReadVector3(L, 3, position_vec)) {
            return DM_LUA_ERROR("position must be vmath.vector3");
        }
        if (!ReadVector3(L, 4, size_vec)) {
            return DM_LUA_ERROR("size must be vmath.vector3");
        }
        ImU32 background_color = (ImU32)luaL_checkinteger(L, 5);

        ImGuizmo::ViewManipulate(
            view_matrix,
            length,
            ImVec2(position_vec[0], position_vec[1]),
            ImVec2(size_vec[0], size_vec[1]),
            background_color
        );
    }

    FloatArrayToMatrix4(view_matrix, view);
    return 0;
}

static int gizmo_GetStyle(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    ImGuizmo::Style& style = ImGuizmo::GetStyle();

    lua_newtable(L);

    lua_pushliteral(L, "TranslationLineThickness");
    lua_pushnumber(L, style.TranslationLineThickness);
    lua_rawset(L, -3);

    lua_pushliteral(L, "TranslationLineArrowSize");
    lua_pushnumber(L, style.TranslationLineArrowSize);
    lua_rawset(L, -3);

    lua_pushliteral(L, "RotationLineThickness");
    lua_pushnumber(L, style.RotationLineThickness);
    lua_rawset(L, -3);

    lua_pushliteral(L, "RotationOuterLineThickness");
    lua_pushnumber(L, style.RotationOuterLineThickness);
    lua_rawset(L, -3);

    lua_pushliteral(L, "ScaleLineThickness");
    lua_pushnumber(L, style.ScaleLineThickness);
    lua_rawset(L, -3);

    lua_pushliteral(L, "ScaleLineCircleSize");
    lua_pushnumber(L, style.ScaleLineCircleSize);
    lua_rawset(L, -3);

    lua_pushliteral(L, "HatchedAxisLineThickness");
    lua_pushnumber(L, style.HatchedAxisLineThickness);
    lua_rawset(L, -3);

    lua_pushliteral(L, "CenterCircleSize");
    lua_pushnumber(L, style.CenterCircleSize);
    lua_rawset(L, -3);

    lua_pushliteral(L, "Colors");
    lua_newtable(L);
    for (int i = 0; i < ImGuizmo::COLOR::COUNT; ++i) {
        const ImVec4& c = style.Colors[i];
        dmScript::PushVector4(L, dmVMath::Vector4(c.x, c.y, c.z, c.w));
        lua_rawseti(L, -2, i);
    }
    lua_rawset(L, -3);

    return 1;
}

static int gizmo_SetStyle(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    if (lua_isnil(L, 1)) {
        return 0;
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    ImGuizmo::Style& style = ImGuizmo::GetStyle();

    lua_pushvalue(L, 1);
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        const char* attr = lua_tostring(L, -2);
        if (attr == NULL) {
            lua_pop(L, 1);
            continue;
        }
        if (strcmp(attr, "TranslationLineThickness") == 0) {
            style.TranslationLineThickness = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "TranslationLineArrowSize") == 0) {
            style.TranslationLineArrowSize = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "RotationLineThickness") == 0) {
            style.RotationLineThickness = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "RotationOuterLineThickness") == 0) {
            style.RotationOuterLineThickness = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "ScaleLineThickness") == 0) {
            style.ScaleLineThickness = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "ScaleLineCircleSize") == 0) {
            style.ScaleLineCircleSize = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "HatchedAxisLineThickness") == 0) {
            style.HatchedAxisLineThickness = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "CenterCircleSize") == 0) {
            style.CenterCircleSize = (float)luaL_checknumber(L, -1);
        } else if (strcmp(attr, "Colors") == 0) {
            if (lua_istable(L, -1)) {
                lua_pushnil(L);
                while (lua_next(L, -2)) {
                    int index = (int)luaL_checkinteger(L, -2);
                    if (index >= 0 && index < ImGuizmo::COLOR::COUNT) {
                        ImVec4 color;
                        if (ReadColor(L, -1, &color)) {
                            style.Colors[index] = color;
                        }
                    }
                    lua_pop(L, 1);
                }
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return 0;
}

static int gizmo_GetStyleColor(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    int index = (int)luaL_checkinteger(L, 1);
    if (index < 0 || index >= ImGuizmo::COLOR::COUNT) {
        return DM_LUA_ERROR("color index out of range");
    }
    ImGuizmo::Style& style = ImGuizmo::GetStyle();
    const ImVec4& c = style.Colors[index];
    dmScript::PushVector4(L, dmVMath::Vector4(c.x, c.y, c.z, c.w));
    return 1;
}

static int gizmo_SetStyleColor(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    int index = (int)luaL_checkinteger(L, 1);
    if (index < 0 || index >= ImGuizmo::COLOR::COUNT) {
        return DM_LUA_ERROR("color index out of range");
    }

    ImVec4 color;
    if (ReadColor(L, 2, &color)) {
        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.Colors[index] = color;
        return 0;
    }

    float r = (float)luaL_checknumber(L, 2);
    float g = (float)luaL_checknumber(L, 3);
    float b = (float)luaL_checknumber(L, 4);
    float a = (float)luaL_checknumber(L, 5);
    ImGuizmo::Style& style = ImGuizmo::GetStyle();
    style.Colors[index] = ImVec4(r, g, b, a);
    return 0;
}

static int gizmo_QuatFromEuler(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    dmVMath::Vector3* v3 = dmScript::CheckVector3(L, 1);
    dmVMath::Quat q = EulerToQuat(*v3);
    dmScript::PushQuat(L, q);
    return 1;
}

static int gizmo_SetContext(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx == NULL) {
        return luaL_error(L, "ImGui context is not initialized");
    }
    ImGuizmo::SetImGuiContext(ctx);
    return 0;
}

static int gizmo_SetDrawlist(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    ImGuizmo::SetDrawlist(NULL);
    return 0;
}

static int gizmo_SetDrawlistForeground(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    return 0;
}

static int gizmo_SetDrawlistBackground(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    return 0;
}

static const luaL_Reg Module_methods[] = {
    {"set_rect", gizmo_SetRect},
    {"set_orthographic", gizmo_SetOrthographic},
    {"enable", gizmo_Enable},
    {"is_over", gizmo_IsOver},
    {"is_over_operation", gizmo_IsOverOperation},
    {"is_over_position", gizmo_IsOverPosition},
    {"is_using", gizmo_IsUsing},
    {"is_using_view_manipulate", gizmo_IsUsingViewManipulate},
    {"is_using_any", gizmo_IsUsingAny},
    {"set_gizmo_size_clip_space", gizmo_SetGizmoSizeClipSpace},
    {"allow_axis_flip", gizmo_AllowAxisFlip},
    {"set_axis_limit", gizmo_SetAxisLimit},
    {"set_axis_mask", gizmo_SetAxisMask},
    {"set_plane_limit", gizmo_SetPlaneLimit},
    {"manipulate", gizmo_Manipulate},
    {"decompose_matrix", gizmo_DecomposeMatrix},
    {"recompose_matrix", gizmo_RecomposeMatrix},
    {"draw_grid", gizmo_DrawGrid},
    {"draw_cubes", gizmo_DrawCubes},
    {"view_manipulate", gizmo_ViewManipulate},
    {"get_style", gizmo_GetStyle},
    {"set_style", gizmo_SetStyle},
    {"get_style_color", gizmo_GetStyleColor},
    {"set_style_color", gizmo_SetStyleColor},
    {"quat_from_euler", gizmo_QuatFromEuler},
    {"set_context", gizmo_SetContext},
    {"set_drawlist", gizmo_SetDrawlist},
    {"set_drawlist_foreground", gizmo_SetDrawlistForeground},
    {"set_drawlist_background", gizmo_SetDrawlistBackground},
    {0, 0}
};

static void lua_setfieldstringint(lua_State* L, const char* key, uint32_t value)
{
    int top = lua_gettop(L);
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
    assert(top == lua_gettop(L));
}

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_setfieldstringint(L, "MODE_WORLD", ImGuizmo::MODE::WORLD);
    lua_setfieldstringint(L, "MODE_LOCAL", ImGuizmo::MODE::LOCAL);
    lua_setfieldstringint(L, "OPERATION_TRANSLATE", ImGuizmo::OPERATION::TRANSLATE);
    lua_setfieldstringint(L, "OPERATION_ROTATE", ImGuizmo::OPERATION::ROTATE);
    lua_setfieldstringint(L, "OPERATION_SCALE", ImGuizmo::OPERATION::SCALE);
    lua_setfieldstringint(L, "OPERATION_SCALEU", ImGuizmo::OPERATION::SCALEU);
    lua_setfieldstringint(L, "OPERATION_UNIVERSAL", ImGuizmo::OPERATION::UNIVERSAL);
    lua_setfieldstringint(L, "OPERATION_TRANSLATE_X", ImGuizmo::OPERATION::TRANSLATE_X);
    lua_setfieldstringint(L, "OPERATION_TRANSLATE_Y", ImGuizmo::OPERATION::TRANSLATE_Y);
    lua_setfieldstringint(L, "OPERATION_TRANSLATE_Z", ImGuizmo::OPERATION::TRANSLATE_Z);
    lua_setfieldstringint(L, "OPERATION_ROTATE_X", ImGuizmo::OPERATION::ROTATE_X);
    lua_setfieldstringint(L, "OPERATION_ROTATE_Y", ImGuizmo::OPERATION::ROTATE_Y);
    lua_setfieldstringint(L, "OPERATION_ROTATE_Z", ImGuizmo::OPERATION::ROTATE_Z);
    lua_setfieldstringint(L, "OPERATION_ROTATE_SCREEN", ImGuizmo::OPERATION::ROTATE_SCREEN);
    lua_setfieldstringint(L, "OPERATION_SCALE_X", ImGuizmo::OPERATION::SCALE_X);
    lua_setfieldstringint(L, "OPERATION_SCALE_Y", ImGuizmo::OPERATION::SCALE_Y);
    lua_setfieldstringint(L, "OPERATION_SCALE_Z", ImGuizmo::OPERATION::SCALE_Z);
    lua_setfieldstringint(L, "OPERATION_BOUNDS", ImGuizmo::OPERATION::BOUNDS);
    lua_setfieldstringint(L, "OPERATION_SCALE_XU", ImGuizmo::OPERATION::SCALE_XU);
    lua_setfieldstringint(L, "OPERATION_SCALE_YU", ImGuizmo::OPERATION::SCALE_YU);
    lua_setfieldstringint(L, "OPERATION_SCALE_ZU", ImGuizmo::OPERATION::SCALE_ZU);

    lua_setfieldstringint(L, "COLOR_DIRECTION_X", ImGuizmo::COLOR::DIRECTION_X);
    lua_setfieldstringint(L, "COLOR_DIRECTION_Y", ImGuizmo::COLOR::DIRECTION_Y);
    lua_setfieldstringint(L, "COLOR_DIRECTION_Z", ImGuizmo::COLOR::DIRECTION_Z);
    lua_setfieldstringint(L, "COLOR_PLANE_X", ImGuizmo::COLOR::PLANE_X);
    lua_setfieldstringint(L, "COLOR_PLANE_Y", ImGuizmo::COLOR::PLANE_Y);
    lua_setfieldstringint(L, "COLOR_PLANE_Z", ImGuizmo::COLOR::PLANE_Z);
    lua_setfieldstringint(L, "COLOR_SELECTION", ImGuizmo::COLOR::SELECTION);
    lua_setfieldstringint(L, "COLOR_INACTIVE", ImGuizmo::COLOR::INACTIVE);
    lua_setfieldstringint(L, "COLOR_TRANSLATION_LINE", ImGuizmo::COLOR::TRANSLATION_LINE);
    lua_setfieldstringint(L, "COLOR_SCALE_LINE", ImGuizmo::COLOR::SCALE_LINE);
    lua_setfieldstringint(L, "COLOR_ROTATION_USING_BORDER", ImGuizmo::COLOR::ROTATION_USING_BORDER);
    lua_setfieldstringint(L, "COLOR_ROTATION_USING_FILL", ImGuizmo::COLOR::ROTATION_USING_FILL);
    lua_setfieldstringint(L, "COLOR_HATCHED_AXIS_LINES", ImGuizmo::COLOR::HATCHED_AXIS_LINES);
    lua_setfieldstringint(L, "COLOR_TEXT", ImGuizmo::COLOR::TEXT);
    lua_setfieldstringint(L, "COLOR_TEXT_SHADOW", ImGuizmo::COLOR::TEXT_SHADOW);
    lua_setfieldstringint(L, "COLOR_COUNT", ImGuizmo::COLOR::COUNT);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result InitializeMyExtension(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, 0, 0, InitializeMyExtension, 0, 0, 0)
