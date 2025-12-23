#define EXTENSION_NAME ImguiGizmo
#define LIB_NAME "ImguiGizmo"
#define MODULE_NAME "imgui_gizmo"

#include <dmsdk/sdk.h>

static const luaL_Reg Module_methods[] = {
    {"__gc", LuaPositionSetterUserdataGC},
    {"add", LuaPositionSetterUserdataAddInstance},
    {"remove", LuaPositionSetterUserdataRemoveInstance},
    {"update", LuaPositionSetterUserdataUpdate},
    {0, 0}
};

static void lua_setfieldstringint(lua_State* L, const char* key, uint32_t value){
    int top = lua_gettop(L);
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
    assert(top == lua_gettop(L));
}

static void LuaInit(lua_State *L) {
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

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result InitializeMyExtension(dmExtension::Params *params) {
    // Init Lua
    LuaInit(params->m_L);

    printf("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeMyExtension, AppFinalizeMyExtension, InitializeMyExtension, 0, 0, FinalizeMyExtension)