// game_data.cpp

// loads stuff from exceloutput, binoutput, and lua

#include "game_data.hpp"

// C++
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <regex>

// rapidjson
#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_NOMEMBERITERATORCLASS 1
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

// lua
extern "C" {
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

// soggy
#include "game_enums.hpp"
#include "vec.hpp"
#include "log.hpp"

//
// structs
//

namespace exceloutput {
	std::unordered_map<int, AvatarData> avatar_datas;
	std::unordered_map<int, AvatarSkillDepotData> avatar_skill_depot_datas;
	std::unordered_map<int, AvatarSkillData> avatar_skill_datas;
	std::unordered_map<int, ItemData> item_datas;
	std::unordered_map<int, ShopPlanData> shop_plan_datas;
	std::unordered_map<int, ShopGoodsData> shop_goods_datas;
	std::unordered_map<int, TalentSkillData> talent_skill_datas;
	std::unordered_map<int, GadgetData> gadget_datas;
	std::unordered_map<int, GadgetData_Level> gadget_level_datas;
	std::unordered_map<int, GadgetData_Monster> gadget_monster_datas;
	std::unordered_map<int, GadgetData_Avatar> gadget_avatar_datas;
	std::unordered_map<int, GadgetData_Equip> gadget_equip_datas;
	std::unordered_map<int, GadgetPropData> gadget_prop_datas;
	std::unordered_map<int, MonsterData> monster_datas;
	std::unordered_map<int, NpcData> npc_datas;
	std::unordered_map<int, SceneData> scene_datas;
	std::unordered_map<int, DungeonData> dungeon_datas;

	std::unordered_map<std::string, std::string> text_map;
}

//void from_json(const nlohmann::json &json, Vec3f &);

namespace binoutput {
	std::unordered_map<int, ConfigScene> scene_points;

	//void from_json(const nlohmann::json &json, ConfigScene &);
	//void from_json(const nlohmann::json &json, ConfigScenePoint &);
}

namespace luares {
	std::unordered_map<int, SceneConfig> scene_configs;
};

//
// excel helper
//

struct Excel {
private:
	char sep;
	std::istream *is;
	std::vector<std::string> headers;
	std::unordered_map<std::string, std::string> fields;
public:
	Excel(std::istream *is, char sep = '\t');
	void next();
	bool eof();
	std::string get_str(const std::string key) const;
	int get_int(const std::string key, int def = 0) const;
	std::vector<int> get_ints(const std::string key) const;
};

Excel::Excel(std::istream *is, char sep) {
	this->sep = sep;
	this->is = is;
	std::string headers_str;
	std::getline(*this->is, headers_str);

	std::istringstream headers_iss(headers_str);
	std::string header;
	while (std::getline(headers_iss, header, this->sep)) {
		this->headers.emplace_back(header);
	}
}
void Excel::next() {
	std::string line_str;
	std::getline(*this->is, line_str);

	std::istringstream line_iss(line_str);
	std::string token;
	int i = 0;
	while (std::getline(line_iss, token, this->sep)) {
		this->fields.insert_or_assign(this->headers.at(i++), token);
	}
}
bool Excel::eof() {
	return this->is->eof();
}

std::string Excel::get_str(const std::string key) const {
	return this->fields.at(key);
}
int Excel::get_int(const std::string key, int def) const {
	auto it = this->fields.find(key);
	if (it == this->fields.cend()) {
		return def;
	}
	const std::string *val = &it->second;
	if (val->empty()) {
		return def;
	}
	return std::stoi(it->second);
}
std::vector<int> Excel::get_ints(const std::string key) const {
	std::vector<int> ints;
	std::istringstream iss(this->fields.at(key));
	std::string token;
	while (std::getline(iss, token, ',')) {
		ints.emplace_back(4);
	}
	return ints;
}

//
// binoutput json stuff
//

void json_read_vec3f(Vec3f *vec3f, const rapidjson::Value &json) {
	const rapidjson::Value::ConstMemberIterator &json_x = json.FindMember("x");
	if (json_x != json.MemberEnd() && json_x->value.IsNumber()) {
		vec3f->x = json_x->value.GetFloat();
	}
	const rapidjson::Value::ConstMemberIterator &json_y = json.FindMember("y");
	if (json_y != json.MemberEnd() && json_y->value.IsNumber()) {
		vec3f->y = json_y->value.GetFloat();
	}
	const rapidjson::Value::ConstMemberIterator &json_z = json.FindMember("z");
	if (json_z != json.MemberEnd() && json_z->value.IsNumber()) {
		vec3f->z = json_z->value.GetFloat();
	}
}

void json_read_config_scene_point(binoutput::ConfigScenePoint *configscenepoint, const rapidjson::Value &json) {
	const rapidjson::Value::ConstMemberIterator &json_type = json.FindMember("$type");
	if (json_type != json.MemberEnd() && json_type->value.IsString()) {
		configscenepoint->$type.assign(json_type->value.GetString());
	}
	const rapidjson::Value::ConstMemberIterator &json_pos = json.FindMember("pos");
	if (json_pos != json.MemberEnd() && json_pos->value.IsObject()) {
		json_read_vec3f(&configscenepoint->pos, json_pos->value);
	}
	const rapidjson::Value::ConstMemberIterator &json_rot = json.FindMember("rot");
	if (json_rot != json.MemberEnd() && json_rot->value.IsObject()) {
		json_read_vec3f(&configscenepoint->rot, json_rot->value);
	}
	const rapidjson::Value::ConstMemberIterator &json_tranpos = json.FindMember("tranPos");
	if (json_tranpos != json.MemberEnd() && json_tranpos->value.IsObject()) {
		json_read_vec3f(&configscenepoint->tranpos, json_tranpos->value);
	}
	const rapidjson::Value::ConstMemberIterator &json_tranrot = json.FindMember("tranRot");
	if (json_tranrot != json.MemberEnd() && json_tranrot->value.IsObject()) {
		json_read_vec3f(&configscenepoint->tranrot, json_tranrot->value);
	}
	const rapidjson::Value::ConstMemberIterator &json_transceneid = json.FindMember("tranSceneId");
	if (json_transceneid != json.MemberEnd() && json_transceneid->value.IsInt()) {
		configscenepoint->transceneid = json_transceneid->value.GetInt();
	}
	const rapidjson::Value::ConstMemberIterator &json_dungeonids = json.FindMember("dungeonIds");
	if (json_dungeonids != json.MemberEnd() && json_dungeonids->value.IsArray()) {
		for (const rapidjson::Value &e : json_dungeonids->value.GetArray()) {
			if (e.IsInt()) {
				configscenepoint->dungeon_ids.push_back(e.GetInt());
			}
		}
	}
	const rapidjson::Value::ConstMemberIterator &json_entrypointid = json.FindMember("entryPointId");
	if (json_entrypointid != json.MemberEnd() && json_entrypointid->value.IsInt()) {
		configscenepoint->dungeon_entry_point_id = json_entrypointid->value.GetInt();
	}
}

void json_read_config_scene(binoutput::ConfigScene *configscene, const rapidjson::Document &json) {
	const rapidjson::Value::ConstMemberIterator json_points = json.FindMember("points");
	if (json_points != json.MemberEnd()) {
		for (rapidjson::Value::ConstMemberIterator it = json_points->value.MemberBegin(); it != json_points->value.MemberEnd(); ++it) {
			if (it->value.IsObject()) {
				int pointid = std::stoi(it->name.GetString());
				binoutput::ConfigScenePoint *configscenepoint = &configscene->points.insert({ pointid, binoutput::ConfigScenePoint() }).first->second;
				json_read_config_scene_point(configscenepoint, it->value);
			}
		}
	}
}

//
// loading data
//

static bool error = false;

template <typename T>
void load_excel(const char *filename, std::unordered_map<int, T> *map, const char *key_column, void (*proc)(T *data, const Excel *excel)) {
	if (!std::filesystem::exists(filename)) {
		soggy_log("error!!!: %s does not exist", filename);
		error = true;
		return;
	}
	std::ifstream ifs(filename);
	Excel excel(&ifs);
	while (!excel.eof()) {
		excel.next();
		int key = excel.get_int(key_column);
		T *data = &(*map)[key];
		data->id = key;
		proc(data, &excel);
	}
}

bool load_game_data() {
	// AvatarData
	load_excel<exceloutput::AvatarData>("resources/exceloutput/AvatarData.tsv", &exceloutput::avatar_datas, "ID", [](exceloutput::AvatarData *avatar, const Excel *excel) {
		avatar->skill_depot_id = excel->get_int("技能库ID");
		avatar->candidate_skill_depot_ids = excel->get_ints("候选技能库ID");
		for (auto it = avatar->candidate_skill_depot_ids.begin(); it != avatar->candidate_skill_depot_ids.end();) {
			if (*it == 0) {
				avatar->candidate_skill_depot_ids.erase(it);
			} else {
				it++;
			}
		}
		avatar->initial_weapon_id = excel->get_int("初始武器");
		avatar->weapon_type = (WeaponType)excel->get_int("武器种类");
		avatar->use_type = (AvatarUseType)excel->get_int("是否使用");
	});

	// AvatarSkillDepotData
	load_excel<exceloutput::AvatarSkillDepotData>("resources/exceloutput/AvatarSkillDepotData.tsv", &exceloutput::avatar_skill_depot_datas, "ID", [](exceloutput::AvatarSkillDepotData *avatar_skill_depot, const Excel *excel) {
		avatar_skill_depot->leader_talent = excel->get_int("队长天赋");
		for (int i = 0; i < 10; i++) {
			int group = excel->get_int("天赋组" + std::to_string(i + 1));
			if (group != 0) {
				avatar_skill_depot->talent_groups.push_back(group);
			}
		}
	});

	// AvatarSkillData
	load_excel<exceloutput::AvatarSkillData>("resources/exceloutput/AvatarSkillData.tsv", &exceloutput::avatar_skill_datas, "ID", [](exceloutput::AvatarSkillData *avatar_skill, const Excel *excel) {
		avatar_skill->energy_type = (ElementType)excel->get_int("消耗能量类型");
		avatar_skill->energy_cost = excel->get_int("消耗能量值");
		avatar_skill->max_charges = excel->get_int("可累积次数", 1);
	});

	// MaterialData
	load_excel<exceloutput::ItemData>("resources/exceloutput/MaterialData.tsv", &exceloutput::item_datas, "ID", [](exceloutput::ItemData *item, const Excel *excel) {
		item->type = (ItemType)excel->get_int("类型");
		item->equip_type = EquipType::EQUIP_NONE;
	});

	// WeaponData
	load_excel<exceloutput::ItemData>("resources/exceloutput/WeaponData.tsv", &exceloutput::item_datas, "ID", [](exceloutput::ItemData *item, const Excel *excel) {
		item->type = (ItemType)excel->get_int("类型");
		item->weapon.gadget_id = excel->get_int("物件ID");
		item->weapon.weapon_type = (WeaponType)excel->get_int("武器阶数");
		item->equip_type = EquipType::EQUIP_WEAPON;
	});

	// ReliquaryData
	load_excel<exceloutput::ItemData>("resources/exceloutput/ReliquaryData.tsv", &exceloutput::item_datas, "ID", [](exceloutput::ItemData *item, const Excel *excel) {
		item->type = (ItemType)excel->get_int("类型");
		item->equip_type = (EquipType)excel->get_int("圣遗物类别");
	});

	// ShopPlanData
	load_excel<exceloutput::ShopPlanData>("resources/exceloutput/ShopPlanData.tsv", &exceloutput::shop_plan_datas, "ID", [](exceloutput::ShopPlanData *shop_plan, const Excel *excel) {
		shop_plan->shop_type = excel->get_int("商店类型");
		shop_plan->goods_id = excel->get_int("商品ID");
	});

	// ShopGoodsData
	load_excel<exceloutput::ShopGoodsData>("resources/exceloutput/ShopGoodsData.tsv", &exceloutput::shop_goods_datas, "商品ID", [](exceloutput::ShopGoodsData *shop_goods, const Excel *excel) {
		shop_goods->result_item_id = excel->get_int("对应物品ID");
		shop_goods->result_item_count = excel->get_int("对应物品数量");
		shop_goods->mora_cost = excel->get_int("消耗金币");
		shop_goods->primogem_cost = excel->get_int("消耗水晶");
		shop_goods->consume_1_item_id = excel->get_int("[消耗物品]1ID");
		shop_goods->consume_1_item_count = excel->get_int("[消耗物品]1数量");
		shop_goods->consume_2_item_id = excel->get_int("[消耗物品]2ID");
		shop_goods->consume_2_item_count = excel->get_int("[消耗物品]2数量");
		shop_goods->max_purchases = excel->get_int("限购数量");
	});

	// TalentSkillData
	load_excel<exceloutput::TalentSkillData>("resources/exceloutput/TalentSkillData.tsv", &exceloutput::talent_skill_datas, "天赋ID", [](exceloutput::TalentSkillData *talent_skill, const Excel *excel) {
		talent_skill->talent_group_id = excel->get_int("天赋组ID");
		talent_skill->rank = excel->get_int("等级");
	});

	// GadgetData
	const auto load_gadgets = [&](const char *filename) {
		load_excel<exceloutput::GadgetData>(filename, &exceloutput::gadget_datas, "ID", [](exceloutput::GadgetData *gadget, const Excel *excel) {
			gadget->default_camp = excel->get_int("默认阵营");
			gadget->is_interactive = (bool)excel->get_int("能否交互");
			gadget->entity_type = (EntityType)excel->get_int("类型");
		});
	};
	load_gadgets("resources/exceloutput/GadgetData.tsv");
	load_gadgets("resources/exceloutput/GadgetData_Avatar.tsv");
	load_gadgets("resources/exceloutput/GadgetData_Level.tsv");
	load_gadgets("resources/exceloutput/GadgetData_Monster.tsv");
	load_gadgets("resources/exceloutput/GadgetData_Equip.tsv");


	// GadgetData_Level
	load_excel<exceloutput::GadgetData_Level>("resources/exceloutput/GadgetData_Level.tsv", &exceloutput::gadget_level_datas, "ID", [](exceloutput::GadgetData_Level* gadget_level, Excel* excel) {
		gadget_level->default_camp = excel->get_int("默认阵营");
	gadget_level->is_interactive = (bool)excel->get_int("能否交互");
		});

	// GadgetData_Monster
	load_excel<exceloutput::GadgetData_Monster>("resources/exceloutput/GadgetData_Monster.tsv", &exceloutput::gadget_monster_datas, "ID", [](exceloutput::GadgetData_Monster* gadget_monster, Excel* excel) {
		gadget_monster->default_camp = excel->get_int("默认阵营");
	gadget_monster->is_interactive = (bool)excel->get_int("能否交互");
		});

	// GadgetData_Avatar
	load_excel<exceloutput::GadgetData_Avatar>("resources/exceloutput/GadgetData_Avatar.tsv", &exceloutput::gadget_avatar_datas, "ID", [](exceloutput::GadgetData_Avatar* gadget_avatar, Excel* excel) {
		gadget_avatar->default_camp = excel->get_int("默认阵营");
	gadget_avatar->is_interactive = (bool)excel->get_int("能否交互");
		});

	// GadgetData_Equip
	load_excel<exceloutput::GadgetData_Equip>("resources/exceloutput/GadgetData_Equip.tsv", &exceloutput::gadget_equip_datas, "ID", [](exceloutput::GadgetData_Equip* gadget_equip, Excel* excel) {
		gadget_equip->default_camp = excel->get_int("默认阵营");
	gadget_equip->is_interactive = (bool)excel->get_int("能否交互");
		});

	// GadgetPropData
	load_excel<exceloutput::GadgetPropData>("resources/exceloutput/GadgetPropData.tsv", &exceloutput::gadget_prop_datas, "ID", [](exceloutput::GadgetPropData* gadgetprop, Excel* excel) {
		});

	// MonsterData
	load_excel<exceloutput::MonsterData>("resources/exceloutput/MonsterData.tsv", &exceloutput::monster_datas, "ID", [](exceloutput::MonsterData *monster, const Excel *excel) {
		monster->equip_1 = excel->get_int("装备1");
		monster->equip_2 = excel->get_int("装备2");
	});

	// NpcData
	load_excel<exceloutput::NpcData>("resources/exceloutput/NpcData.tsv", &exceloutput::npc_datas, "ID", [](exceloutput::NpcData *npc, const Excel *excel) {
	});

	// SceneData
	load_excel<exceloutput::SceneData>("resources/exceloutput/SceneData.tsv", &exceloutput::scene_datas, "ID", [](exceloutput::SceneData *scene, const Excel *excel) {
		scene->type = (SceneType)excel->get_int("类型");
	});

	// DungeonData
	load_excel<exceloutput::DungeonData>("resources/exceloutput/DungeonData.tsv", &exceloutput::dungeon_datas, "ID", [](exceloutput::DungeonData *dungeon, const Excel *excel) {
		dungeon->scene_id = excel->get_int("场景ID");
		dungeon->daily_limit = excel->get_int("每天准入次数");
	});

	// scene configs
	if (!std::filesystem::exists("resources/binoutput/Scene/Point")) {
		soggy_log("error!!!: resources/binoutput/Scene/Point does not exist");
		error = true;
	} else {
		static const std::regex re_scene_points(R"(^scene(\d+)_point\.json$)");
		for (const std::filesystem::directory_entry &dirent : std::filesystem::directory_iterator("resources/binoutput/Scene/Point")) {
			std::filesystem::path path = dirent.path();
			std::smatch m;
			std::string filename = path.filename().string();
			if (std::regex_match(filename, m, re_scene_points)) {
				std::string s = m[1].str();
				int sceneid = std::stoi(s);

				rapidjson::Document json;
				{
					std::ifstream ifs(path);
					rapidjson::IStreamWrapper isw(ifs);
					json.ParseStream(isw);
				}

				binoutput::ConfigScene *configscene = &binoutput::scene_points.insert({ sceneid, binoutput::ConfigScene() }).first->second;
				json_read_config_scene(configscene, json);
			}
		}

		for (auto &it : binoutput::scene_points) {
			binoutput::ConfigScene *scene = &it.second;
			for (auto &it : scene->points) {
				int exit_pointid = it.first;
				binoutput::ConfigScenePoint *exit_point = &it.second;
				if (exit_point->$type == "DungeonExit") {
					binoutput::ConfigScenePoint *entry_point = &scene->points.at(exit_point->dungeon_entry_point_id);
					entry_point->dungeon_exit_point_id = exit_pointid;
				}
			}
		}
	}

	return error;
}

void soglua_get_float(lua_State *L, const char *key, float *num) {
	if (lua_getfield(L, -1, key) == LUA_TNUMBER) {
		*num = lua_tonumber(L, -1);
	}
	lua_pop(L, 1);
}

void soglua_get_int(lua_State *L, const char *key, int *num) {
	if (lua_getfield(L, -1, key) == LUA_TNUMBER) {
		*num = lua_tointeger(L, -1);
	}
	lua_pop(L, 1);
}

void soglua_get_bool(lua_State *L, const char *key, bool *num) {
	if (lua_getfield(L, -1, key) == LUA_TBOOLEAN) {
		*num = lua_toboolean(L, -1);
	}
	lua_pop(L, 1);
}

void soglua_get_vec2f(lua_State *L, const char *key, Vec2f *vec) {
	lua_pushstring(L, key);
	if (lua_gettable(L, -2) == LUA_TTABLE) {
		soglua_get_float(L, "x", &vec->x);
		soglua_get_float(L, "z", &vec->z);
	}
	lua_pop(L, 1);
}

void soglua_get_vec3f(lua_State *L, const char *key, Vec3f *vec) {
	lua_pushstring(L, key);
	if (lua_gettable(L, -2) == LUA_TTABLE) {
		soglua_get_float(L, "x", &vec->x);
		soglua_get_float(L, "y", &vec->y);
		soglua_get_float(L, "z", &vec->z);
	}
	lua_pop(L, 1);
}

void soglua_set_int(lua_State *L, const char *key, lua_Integer i) {
	lua_pushinteger(L, i);
	lua_setfield(L, -2, key);
}

bool load_scene_script_data(int sceneid) {
	if (luares::scene_configs.find(sceneid) != luares::scene_configs.end()) {
		// already loaded
		return true;
	}

	soggy_log("read scene script data for sceneid %d", sceneid);

	// lua init
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	// add ./resources/lua to require path
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	std::string path(lua_tostring(L, -1));
	path += ";./resources/lua/?.lua";
	lua_pop(L, 1);
	lua_pushstring(L, path.c_str());
	lua_setfield(L, -2, "path");
	lua_pop(L, 1);

	if (luaL_dofile(L, "resources/lua/Config/DefineCommon.lua") != LUA_OK) {
		soggy_log("error!!!: resources/lua/Config/DefineCommon.lua does not exist");
		return false;
	}

	// hack? the enum for GadgetState in the luas is completely wrong??
	assert(lua_getglobal(L, "GadgetState") == LUA_TTABLE);
	soglua_set_int(L, "Default", GadgetState::GADGET_STATE_Default);
	soglua_set_int(L, "ChestLocked", GadgetState::GADGET_STATE_ChestLocked);
	soglua_set_int(L, "GearStart", GadgetState::GADGET_STATE_GearStart);
	soglua_set_int(L, "GearStop", GadgetState::GADGET_STATE_GearStop);
	soglua_set_int(L, "CrystalResonate1", GadgetState::GADGET_STATE_CrystalResonate1);
	soglua_set_int(L, "CrystalResonate2", GadgetState::GADGET_STATE_CrystalResonate2);
	soglua_set_int(L, "CrystalExplode", GadgetState::GADGET_STATE_CrystalExplode);
	soglua_set_int(L, "CrystalDrain", GadgetState::GADGET_STATE_CrystalDrain);
	lua_pop(L, 1);

	{
		std::string scene_lua_path = "resources/lua/Scene/" + std::to_string(sceneid) + "/scene" + std::to_string(sceneid) + ".lua";
		if (luaL_dofile(L, scene_lua_path.c_str()) != LUA_OK) {
			soggy_log("error!!!: %s does not exist", scene_lua_path.c_str());
			return false;
		}
	}
	luares::SceneConfig *scene_config = &luares::scene_configs[sceneid];

	// scene_config
	if (lua_getglobal(L, "scene_config") == LUA_TTABLE) {
		soglua_get_vec3f(L, "born_pos", &scene_config->born_pos);
		soglua_get_vec3f(L, "born_rot", &scene_config->born_rot);
		soglua_get_float(L, "die_y", &scene_config->die_y);
	}
	lua_pop(L, 1);

	// blocks
	std::vector<int> block_ids;
	if (lua_getglobal(L, "blocks") == LUA_TTABLE) {
		lua_pushnil(L);
		while (lua_next(L, -2)) { // key=-2, value=-1
			int block_id = lua_tointeger(L, -1);
			block_ids.push_back(block_id);
			luares::SceneBlock *block = &scene_config->blocks[block_id];
			block->id = block_id;
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	// block_rects
	if (lua_getglobal(L, "block_rects") == LUA_TTABLE) {
		lua_pushnil(L);
		int i = 0;
		while (lua_next(L, -2)) { // key=-2, value=-1
			int block_id = block_ids[i++];
			luares::SceneBlock *block = &scene_config->blocks[block_id];
			soglua_get_vec2f(L, "min", &block->min);
			soglua_get_vec2f(L, "max", &block->max);
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	// read each block
	for (auto &it : scene_config->blocks) {
		int block_id = it.first;
		luares::SceneBlock *block = &it.second;

		{
			std::string block_lua_path = "resources/lua/Scene/" + std::to_string(sceneid) + "/scene" + std::to_string(sceneid) + "_block" + std::to_string(block_id) + ".lua";
			if (luaL_dofile(L, block_lua_path.c_str()) != LUA_OK) {
				soggy_log("error!!!: %s does not exist", block_lua_path.c_str());
				return false;
			}
		}

		if (lua_getglobal(L, "groups") == LUA_TTABLE) {
			lua_pushnil(L);
			while (lua_next(L, -2)) { // key=-2, value=-1
				int group_id = 0;
				soglua_get_int(L, "id", &group_id);
				luares::SceneGroup *group = &block->groups[group_id];
				group->id = group_id;

				soglua_get_int(L, "area", &group->area);
				soglua_get_vec3f(L, "pos", &group->pos);

				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		// read each group
		for (auto &it : block->groups) {
			int group_id = it.first;
			luares::SceneGroup *group = &it.second;

			{
				std::string group_lua_path = "resources/lua/Scene/" + std::to_string(sceneid) + "/scene" + std::to_string(sceneid) + "_group" + std::to_string(group_id) + ".lua";
				if (luaL_dofile(L, group_lua_path.c_str()) != LUA_OK) {
					soggy_log("error!!!: %s does not exist", group_lua_path.c_str());
					return false;
				}
			}

			// gadgets
			if (lua_getglobal(L, "gadgets") == LUA_TTABLE) {
				lua_pushnil(L);
				while (lua_next(L, -2)) { // key=-2, value=-1
					luares::SceneEntity *gadget = &group->gadgets.emplace_back();

					soglua_get_vec3f(L, "pos", &gadget->pos);
					soglua_get_vec3f(L, "rot", &gadget->rot);
					soglua_get_int(L, "config_id", &gadget->config_id);
					soglua_get_int(L, "gadget_id", &gadget->gadget.gadget_id);
					soglua_get_int(L, "level", &gadget->gadget.level);
					soglua_get_int(L, "route_id", &gadget->gadget.route_id);
					soglua_get_bool(L, "save_route", &gadget->gadget.save_route);
					soglua_get_bool(L, "persistent", &gadget->gadget.persistent);
					soglua_get_int(L, "state", (int *)&gadget->gadget.state);
					soglua_get_int(L, "type", (int *)&gadget->gadget.type);
					soglua_get_int(L, "point_type", &gadget->gadget.point_type);
					soglua_get_int(L, "owner", &gadget->gadget.owner);

					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);

			// monsters
			if (lua_getglobal(L, "monsters") == LUA_TTABLE) {
				lua_pushnil(L);
				while (lua_next(L, -2)) { // key=-2, value=-1
					luares::SceneEntity *monster = &group->monsters.emplace_back();

					soglua_get_vec3f(L, "pos", &monster->pos);
					soglua_get_vec3f(L, "rot", &monster->rot);
					soglua_get_int(L, "config_id", &monster->config_id);
					soglua_get_int(L, "monster_id", &monster->monster.monster_id);
					soglua_get_int(L, "level", &monster->monster.level);
					soglua_get_int(L, "drop_id", &monster->monster.drop_id);
					soglua_get_bool(L, "disableWander", &monster->monster.disable_wander);

					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);

			// npcs
			if (lua_getglobal(L, "npcs") == LUA_TTABLE) {
				lua_pushnil(L);
				while (lua_next(L, -2)) { // key=-2, value=-1
					luares::SceneEntity *npc = &group->npcs.emplace_back();

					soglua_get_vec3f(L, "pos", &npc->pos);
					soglua_get_vec3f(L, "rot", &npc->rot);
					soglua_get_int(L, "config_id", &npc->config_id);
					soglua_get_int(L, "npc_id", &npc->npc.npc_id);
					soglua_get_int(L, "pointID", &npc->npc.point_id);

					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);
		}
	}

	lua_close(L);

	return true;
}
