#include "randomizer.h"

#include <chrono>
#include <ctime>
#include <cstdarg>
#include <string>
#include <math.h>
#include "../obse/obse/obse/ModTable.h"
#include "../obse/obse/obse/GameData.h"


/*#define FORMMAPADDR 0x00B0613C

NiTMapBase<unsigned int, TESForm*>* allObjects = (NiTMapBase<unsigned int, TESForm*>*)FORMMAPADDR;
*/

std::map<UInt32, std::vector<UInt32>> allWeapons;
std::map<UInt32, std::vector<UInt32>> allClothingAndArmor;
std::map<UInt32, std::vector<UInt32>> allGenericItems;
std::vector<UInt32> allCreatures;
std::vector<UInt32> allItems;
std::set<UInt32> allAdded;

std::list<TESObjectREFR*> toRandomize;
std::map<TESObjectREFR*, UInt32> restoreFlags;

TESForm* obrnFlag = NULL;

static int indent = 0;
int clothingRanges[CRANGE_MAX];
int weaponRanges[WRANGE_MAX];

int oUseEssentialCreatures = 0;
int oExcludeQuestItems = 1;
int oRandCreatures = 1;
int oAddItems = 1;
int oDeathItems = 1;
int oWorldItems = 1;
int oRandInventory = 1;

bool skipMod[0xFF] = { 0 };
UInt8 randId = 0xFF;

UInt8 GetModIndexShifted(const std::string& name) {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	UInt8 id = ModTable::Get().GetModIndex(name);
	if (id == 0xFF) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s - id = 255", __func__);
#endif
		return id;
	}
	const ModEntry** activeModList = (*g_dataHandler)->GetActiveModList();
	if (stricmp(activeModList[0]->data->name, "oblivion.esm") && stricmp(activeModList[id]->data->name, name.c_str()) == 0) {
		++id;
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
	return id;
}

void MESSAGE(const char* format, ...) {
	using namespace std::chrono;
	system_clock::time_point currentTime = system_clock::now();
	char buffer1[80];

	auto secondsPart = time_point_cast<seconds>(currentTime);
	auto millis = duration_cast<milliseconds>(currentTime - secondsPart);

	std::time_t tt = system_clock::to_time_t(currentTime);
	auto timeinfo = localtime(&tt);
	strftime(buffer1, 80, "%F %H:%M:%S", timeinfo);
	snprintf(buffer1 + strlen(buffer1), sizeof(buffer1) - strlen(buffer1), ":%03lld", static_cast<long long>(millis.count()));

	auto ltime = buffer1;

	// Create a buffer for the formatted string using std::string
	char buffer[1024] = ""; // Adjust the size as needed
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	// Combine timestamp and formatted string using std::string
	std::string result = ltime;
	result += " - ";
	result += buffer;

	// Pass the const char* to _MESSAGE function
	_MESSAGE(result.c_str());
}

void TRACEMESSAGE(TESForm* f, const char* format, ...)
{
	if (indent < 0)
		indent = 0;
	
	// Create a buffer for the formatted string using std::string
	char buffer[1024]; // Adjust the size as needed
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	char buffer1[1024] = "";
	if (f != nullptr)
	{
		std::sprintf(buffer1, "\t indent: %8i \t mod: %4i \t ref: %08X \t ", indent, f->GetModIndex(), f->refID);
	}

	std::string result(buffer1);
	for (int n = 0; n < indent; n++)
		result += "==";
	result += buffer;

	// Pass the const char* to _MESSAGE function
	
	if (result.find("Start:") || result.find("start:"))
		indent++;
	if (result.find("End  :") || result.find("end  :"))
		indent--;
	MESSAGE(result.c_str());
}

void TRACEMESSAGE(const char* format, ...)
{
	return;
	if (indent < 0)
		indent = 0;

	// Create a buffer for the formatted string using std::string
	char buffer[1024]; // Adjust the size as needed
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	std::string result = "";

	for (int n = 0; n < indent; n++)
		result += "==";
	result += buffer;

	// Pass the const char* to _MESSAGE function

	if (result.find("Start:") || result.find("start:"))
		indent++;
	if (result.find("End  :") || result.find("end  :"))
		indent--;
	MESSAGE(result.c_str());
}

void InitModExcludes() {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	randId = GetModIndexShifted("Randomizer.esp");
	for (int i = 0; i < 0xFF; ++i) {
		skipMod[i] = false;
	}
	if (randId != 0xFF) {
		MESSAGE("Randomizer.esp's ID is %02X", randId);
		skipMod[randId] = true;
	}
	else {
		_ERROR("Couldn't find Randomizer.esp's mod ID. Make sure that you did not rename the plugin file.");
	}
	FILE* f = fopen("Data/RandomizerSkip.cfg", "r");
	if (f == NULL) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s failed to open file", __func__);
#endif
		return;
	}
	char buf[256] = { 0 };
	for (int i = 0; fscanf(f, "%255[^\n]\n", buf) > 0 /* != EOF*/ && i < 0xFF; ++i) {
		//same reasoning as in InitConfig()
		UInt8 id = GetModIndexShifted(buf);
		if (id == 0xFF) {
			_WARNING("Could not get mod ID for mod %s", buf);
			continue;
		}
		skipMod[id] = true;
		MESSAGE("Skipping mod %s\n", buf);
	}
	fclose(f);
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
}

char* cfgeol(char* s) {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	for (char *p = s; *p != 0; ++p) {
		if (*p == ';' || *p == '\t' || *p == '\n' || *p == '\r') {
#ifdef TRACE
			TRACEMESSAGE("End  : %s", __func__);
#endif
			return p;
		}
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s returned null", __func__);
#endif
	return NULL;
}

#define OBRN_VAR2STR(x) #x
#define OBRN_PARSECONFIGLINE(line, var, str, len) \
if (strncmp(line, str, len) == 0 && isspace(line[len])) {\
	var = atoi(line + len + 1); \
	continue; \
}

#define OBRN_CONFIGLINE(line, var) \
{\
	int len = strlen(OBRN_VAR2STR(set ZZZOBRNRandomQuest.##var to));\
	OBRN_PARSECONFIGLINE(line, var, OBRN_VAR2STR(set ZZZOBRNRandomQuest.##var to), len);\
}

//this is not an elegant solution but i dont want to split the config into two files
void InitConfig() {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	FILE* f = fopen("Data/Randomizer.cfg", "r");
	if (f == NULL) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s unable to open file", __func__);
#endif
		return;
	}
	char buf[256] = { 0 };
	for (int i = 0; fscanf(f, "%255[^\n]\n", buf) > 0/* != EOF*/ && i < 512; ++i) { 
		//the number of iterations is not necessary given the fscanf > 0 check but I'll sleep better knowing that this will never cause an infinite loop
		if (char* p = cfgeol(buf)) {
			*p = 0;
		}
		OBRN_CONFIGLINE(buf, oUseEssentialCreatures);
		OBRN_CONFIGLINE(buf, oExcludeQuestItems);
		OBRN_CONFIGLINE(buf, oRandCreatures);
		OBRN_CONFIGLINE(buf, oAddItems);
		OBRN_CONFIGLINE(buf, oDeathItems);
		OBRN_CONFIGLINE(buf, oWorldItems);
		OBRN_CONFIGLINE(buf, oRandInventory);
		//set ZZZOBRNRandomQuest.oUseEssentialCreatures to 0
		//set ZZZOBRNRandomQuest.oExcludeQuestItems to 1
	}
	fclose(f);
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
}

int clothingKeys[] = { kSlot_UpperBody, kSlot_UpperHand, kSlot_UpperLower, kSlot_UpperLowerFoot, kSlot_UpperLowerHandFoot, kSlot_UpperLowerHand, -1 };
void fillUpClothingRanges() {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	for (int i = 0; i < CRANGE_MAX; ++i) {
		clothingRanges[i] = 0;
	}
	for (int* k = &clothingKeys[0], i = -1; *k != -1; ++k) {
		auto it = allClothingAndArmor.find(*k);//allClothingAndArmor.find(TESBipedModelForm::SlotForMask(*k));
		if (it == allClothingAndArmor.end()) {
			continue;
		}
		clothingRanges[++i] = it->second.size();
	}
	for (int i = 1; i < CRANGE_MAX; ++i) {
		clothingRanges[i] += clothingRanges[i - 1];
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
}

void fillUpWpRanges() {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	int wpKeys[] = { TESObjectWEAP::kType_BladeOneHand, TESObjectWEAP::kType_BluntOneHand, TESObjectWEAP::kType_Staff, TESObjectWEAP::kType_Bow, -1 };
	int maxUnarmed = 0;
	for (int i = 0; i < WRANGE_MAX; ++i) {
		weaponRanges[i] = 0;
	}
	for (int* k = &wpKeys[0], i = -1; *k != -1; ++k) {
		auto it = allWeapons.find(*k);
		if (it == allWeapons.end()) {
			continue;
		}
		weaponRanges[++i] = it->second.size();
	}
	weaponRanges[UNARMED] = weaponRanges[BOWS] * (10.0 / 9.0);
	for (int i = 1; i < WRANGE_MAX; ++i) {
		weaponRanges[i] += weaponRanges[i - 1];
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
}

bool getRandomForKey(ItemMapPtr map, const UInt32 key, UInt32& out) {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	auto it = map->find(key);
	if (it == map->end() || !it->second.size()) {
		_ERROR("Couldn't find key %u for map %s\n", key, (map == &allWeapons ? "weapons" : (map == &allClothingAndArmor ? "clothing & armor" : "generic")));
#ifdef TRACE
		TRACEMESSAGE("End  : %s - could not find key", __func__);
#endif
		return false;
	}
	out = it->second[rand() % it->second.size()];
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
	return true;
}

void addOrAppend(ItemMapPtr map, const UInt32 key, const UInt32 value) {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	allAdded.insert(value);
	auto it = map->find(key);
	if (it == map->end()) {
		std::vector<UInt32> itemList;
		itemList.push_back(value);
		map->insert(std::make_pair(key, itemList));
	}
	else {
		it->second.push_back(value);
	}
	
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
}

bool isQuestItem(TESForm* item) {
#ifdef TRACE
	TRACEMESSAGE(item, "Start: %s", __func__);
#endif
	if (item->IsQuestItem()) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s isQuestItem", __func__);
#endif
		return true;
	}
	TESScriptableForm* scriptForm = OBLIVION_CAST(item, TESForm, TESScriptableForm);
	if (scriptForm != NULL && scriptForm->script != NULL) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s, is scripted", __func__);
#endif
		return true;
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
	return false;
}

bool tryToAddForm(TESForm* f) {

#ifdef TRACE
	TRACEMESSAGE(f,"Start: %s", __func__);
#endif
	if (f == nullptr)
		return false;

	ItemMapPtr ptr = NULL;
	UInt32 key = 0xFFFFFFFF;
	const char* name = GetFullName(f);
	if (name[0] == '<') {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s, name is less than", __func__);
#endif
		return false;
	}

	if (obrnFlag == NULL && f->GetModIndex() == randId && f->GetFormType() == kFormType_Misc && strcmp(name, "You should not see this") == 0) {
		obrnFlag = f;
		MESSAGE("OBRN Flag found as %08X", f->refID);
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s, obrn flag not found", __func__);
#endif
		return false;
	}

	if (f->GetModIndex() == 0xFF || skipMod[f->GetModIndex()]) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s skipped", __func__);
#endif
		return false;
	}
	if (f->GetFormType() != kFormType_Creature && f->IsQuestItem() && oExcludeQuestItems) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s quest items excluded", __func__);
#endif
		return false;
	}
	if (allAdded.find(f->refID) != allAdded.end()) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s, refId not found", __func__);
#endif
		return false;
	}
	if (strncmp(name, "aaa", 3) == 0) {
		//exception for some test objects that typically don't even have a working model
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s test objects without working model", __func__);
#endif
		return false;
	}
	switch (f->GetFormType()) {
	case kFormType_Creature:
	{
		TESCreature* critter = OBLIVION_CAST(f, TESForm, TESCreature);
		TESScriptableForm* scriptForm = OBLIVION_CAST(f, TESForm, TESScriptableForm);
		if (scriptForm != NULL && critter != NULL) {
			if ((oUseEssentialCreatures || (!critter->actorBaseData.IsEssential()) && 
				(!oExcludeQuestItems || scriptForm->script == NULL))) {
				const char* model = critter->modelList.modelList.Info();
				if (model == NULL) {
					model = "";
				}
				//hardcoded exception for SI grummites without a working model + excluding some test creatures
				if (strstr(name, "Test") == NULL && (strncmp(name, "Grummite Whelp", 14) ||
					strncmp(model, "GobLegs01.NIF", 13))) {
					allCreatures.push_back(f->refID);
					allAdded.insert(f->refID);
#ifdef TRACE
					TRACEMESSAGE(f,"End  : %s grummites exception", __func__);
#endif
					return true;
				}
			}
		}
#ifdef DEBUG
		MESSAGE("Skipping %08X (%s)", f->refID, name);
#endif
		break;
	}
	case kFormType_Armor:
	case kFormType_Clothing:
	{
		ptr = &allClothingAndArmor;
		if (f->GetFormType() == kFormType_Armor) {
			TESObjectARMO* armor = OBLIVION_CAST(f, TESForm, TESObjectARMO);
			key = armor->bipedModel.partMask & ~kSlot_Tail; //yeah... we kinda dont care about this slot
		}
		else {
			TESObjectCLOT* clothing = OBLIVION_CAST(f, TESForm, TESObjectCLOT);
			key = clothing->bipedModel.partMask;
		}
		if (key & kSlot_RightRing) {
			key = kSlot_LeftRing;
		}
		if (key & kSlot_Hair || key & kSlot_Head) {
			key = kSlot_Head;
		}
		break;
	}
	case kFormType_Weapon:
	{
		ptr = &allWeapons;
		TESObjectWEAP* weapon = OBLIVION_CAST(f, TESForm, TESObjectWEAP);
		key = weapon->type;
		if (key == TESObjectWEAP::kType_BladeTwoHand) {
			key = TESObjectWEAP::kType_BladeOneHand;
		}
		else if (key == TESObjectWEAP::kType_BluntTwoHand) {
			key = TESObjectWEAP::kType_BluntOneHand;
		}
		break;
	}
	case kFormType_Apparatus:
	case kFormType_Book:
	case kFormType_Ingredient:
	case kFormType_Misc:
	case kFormType_Ammo:
	case kFormType_SoulGem:
	case kFormType_Key:
	case kFormType_AlchemyItem:
	case kFormType_SigilStone:
		ptr = &allGenericItems;
		key = f->GetFormType();
	default:
		break;
	}
	if (ptr != NULL && key != 0xFFFFFFFF) {
		addOrAppend(ptr, key, f->refID);
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s  return true", __func__);
#endif
		return true;
	}
#ifdef TRACE
	TRACEMESSAGE(f,"End  : %s return false", __func__);
#endif
	return false;
}

bool refIsItem(TESObjectREFR* ref) {
#ifdef TRACE
	TRACEMESSAGE(ref,"Start: %s", __func__);
#endif
	switch (ref->baseForm->GetFormType()) {
	case kFormType_Armor:
	case kFormType_Clothing:
	case kFormType_Weapon:
	case kFormType_Apparatus:
	case kFormType_Book:
	case kFormType_Ingredient:
	case kFormType_Misc:
	case kFormType_Ammo:
	case kFormType_SoulGem:
	case kFormType_Key:
	case kFormType_AlchemyItem:
	case kFormType_SigilStone:
#ifdef TRACE
		TRACEMESSAGE(ref,"End  : %s True", __func__);
#endif
		return true;
	default:
#ifdef TRACE
		TRACEMESSAGE(ref,"End  : %s False", __func__);
#endif
		return false;
	}
}

bool itemIsEquippable(TESForm* item) {
#ifdef TRACE
	TRACEMESSAGE(item,"Start: %s", __func__);
#endif
	switch (item->GetFormType()) {
	case kFormType_Armor:
	case kFormType_Clothing:
	case kFormType_Weapon:
	case kFormType_Ammo:
#ifdef TRACE
		TRACEMESSAGE(item,"End  : %s true", __func__);
#endif
		return true;
	default:
#ifdef TRACE
		TRACEMESSAGE(item,"End  : %s false", __func__);
#endif
		return false;
	}
}

const char* formToString[] = {
	TESFORM2STRING(kFormType_None),
	TESFORM2STRING(kFormType_TES4),
	TESFORM2STRING(kFormType_Group),
	TESFORM2STRING(kFormType_GMST),
	TESFORM2STRING(kFormType_Global),
	TESFORM2STRING(kFormType_Class),
	TESFORM2STRING(kFormType_Faction),
	TESFORM2STRING(kFormType_Hair),
	TESFORM2STRING(kFormType_Eyes),
	TESFORM2STRING(kFormType_Race),
	TESFORM2STRING(kFormType_Sound),
	TESFORM2STRING(kFormType_Skill),
	TESFORM2STRING(kFormType_Effect),
	TESFORM2STRING(kFormType_Script),
	TESFORM2STRING(kFormType_LandTexture),
	TESFORM2STRING(kFormType_Enchantment),
	TESFORM2STRING(kFormType_Spell),
	TESFORM2STRING(kFormType_BirthSign),
	TESFORM2STRING(kFormType_Activator),
	TESFORM2STRING(kFormType_Apparatus),
	TESFORM2STRING(kFormType_Armor),
	TESFORM2STRING(kFormType_Book),
	TESFORM2STRING(kFormType_Clothing),
	TESFORM2STRING(kFormType_Container),
	TESFORM2STRING(kFormType_Door),
	TESFORM2STRING(kFormType_Ingredient),
	TESFORM2STRING(kFormType_Light),
	TESFORM2STRING(kFormType_Misc),
	TESFORM2STRING(kFormType_Stat),
	TESFORM2STRING(kFormType_Grass),
	TESFORM2STRING(kFormType_Tree),
	TESFORM2STRING(kFormType_Flora),
	TESFORM2STRING(kFormType_Furniture),
	TESFORM2STRING(kFormType_Weapon),
	TESFORM2STRING(kFormType_Ammo),
	TESFORM2STRING(kFormType_NPC),
	TESFORM2STRING(kFormType_Creature),
	TESFORM2STRING(kFormType_LeveledCreature),
	TESFORM2STRING(kFormType_SoulGem),
	TESFORM2STRING(kFormType_Key),
	TESFORM2STRING(kFormType_AlchemyItem),
	TESFORM2STRING(kFormType_SubSpace),
	TESFORM2STRING(kFormType_SigilStone),
	TESFORM2STRING(kFormType_LeveledItem),
	TESFORM2STRING(kFormType_SNDG),
	TESFORM2STRING(kFormType_Weather),
	TESFORM2STRING(kFormType_Climate),
	TESFORM2STRING(kFormType_Region),
	TESFORM2STRING(kFormType_Cell),
	TESFORM2STRING(kFormType_REFR),
	TESFORM2STRING(kFormType_ACHR),
	TESFORM2STRING(kFormType_ACRE),
	TESFORM2STRING(kFormType_PathGrid),
	TESFORM2STRING(kFormType_WorldSpace),
	TESFORM2STRING(kFormType_Land),
	TESFORM2STRING(kFormType_TLOD),
	TESFORM2STRING(kFormType_Road),
	TESFORM2STRING(kFormType_Dialog),
	TESFORM2STRING(kFormType_DialogInfo),
	TESFORM2STRING(kFormType_Quest),
	TESFORM2STRING(kFormType_Idle),
	TESFORM2STRING(kFormType_Package),
	TESFORM2STRING(kFormType_CombatStyle),
	TESFORM2STRING(kFormType_LoadScreen),
	TESFORM2STRING(kFormType_LeveledSpell),
	TESFORM2STRING(kFormType_ANIO),
	TESFORM2STRING(kFormType_WaterForm),
	TESFORM2STRING(kFormType_EffectShader),
	TESFORM2STRING(kFormType_TOFT)
};

const char* FormToString(int form) {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	if (form >= 0 && form < sizeof(formToString) / sizeof(const char*)) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s", __func__);
#endif
		return formToString[form];
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s  Unknown", __func__);
#endif
	return "Unknown";
}

int myrand(int min, int max) {
	return ((double)rand() / (double)RAND_MAX) * (max - min) + min;
}

bool getFormsFromLeveledList(TESLevItem* lev, std::map<UInt32, TESForm*>& forms) {
#ifdef TRACE
	TRACEMESSAGE(lev, "Start: %s", __func__);
#endif
	auto data = lev->leveledList.list.Info();
	auto next = lev->leveledList.list.Next();
	while (data != NULL) {
		if (data->form->GetFormType() == kFormType_LeveledItem) {
			if (!getFormsFromLeveledList(OBLIVION_CAST(data->form, TESForm, TESLevItem), forms)) {
#ifdef TRACE
				TRACEMESSAGE(lev,"End  : %s not get forms", __func__);
#endif
				return false;
			}
		}
		else {
			if (isQuestItem(data->form) && oExcludeQuestItems) {
#ifdef TRACE
				TRACEMESSAGE(lev,"End  : %s exclude quest items", __func__);
#endif
				return false;
			}
			forms.insert(std::make_pair(data->form->GetFormType(), data->form));
		}
		if (next == NULL) {
			break;
		}
		data = next->Info();
		next = next->Next();
	}
#ifdef TRACE
	TRACEMESSAGE(lev,"End  : %s", __func__);
#endif
	return true;
}

void getInventoryFromTESLevItem(TESLevItem* lev, std::map<TESForm*, int>& itemList, bool addQuestItems) {
#ifdef _DEBUG
	TRACEMESSAGE(lev,"Start: %s", __func__);
#endif
	auto data = lev->leveledList.list.Info();
	auto next = lev->leveledList.list.Next();
	while (data != NULL) {
#ifdef _DEBUG
		MESSAGE("data not = null");
#endif
		if (data->form->GetFormType() == kFormType_LeveledItem) {
#ifdef _DEBUG
			MESSAGE("for %08X is a leveled item, count = %lu", data->form, data->count);
#endif
			getInventoryFromTESLevItem(OBLIVION_CAST(data->form, TESForm, TESLevItem), itemList, addQuestItems);
		}
		else {
			if (!isQuestItem(data->form) || addQuestItems) {
#ifdef _DEBUG
				MESSAGE("looking for item %08X count = %lu", data->form, data->count);
#endif
				auto it = itemList.find(data->form);
#ifdef _DEBUG
				MESSAGE("found dataform at %08X and put into it at %08X with a count of %i", data->form, it, data->count);
#endif
				UINT16 count = 1 > data->count ? 1 : data->count;
				if (it == itemList.end()) {
#ifdef _DEBUG
					MESSAGE("inserting %08X", data->form);
#endif
					itemList.insert(std::make_pair(data->form, count));
				}
				else {
#ifdef _DEBUG
					MESSAGE("Incrementing %08X from %i by %i for %i", data->form, it->second, count, it->second + count);
#endif
					it->second += count;
				}
			}
		}
		if (next == NULL) {
			break;
		}
		data = next->Info();
		next = next->Next();
	}
#ifdef _DEBUG
	TRACEMESSAGE(lev,"End  : %s", __func__);
#endif
}

std::pair<TESForm*, int> getFormFromTESLevItem(TESLevItem* lev, bool addQuestItems) {
#ifdef _DEBUG	
	TRACEMESSAGE(lev,"Start: %s", __func__);
#endif
	std::map<TESForm*, int> itemList;
	getInventoryFromTESLevItem(lev, itemList, addQuestItems);
	if (!itemList.size()) {
#ifdef _DEBUG
		TRACEMESSAGE(lev,"End  : %s item list size = 0", __func__);
#endif
		return std::make_pair((TESForm*)NULL, (int)0);
	}
	int r = myrand(0, itemList.size() - 1), i = -1;
#ifdef _DEBUG
	MESSAGE("looking for %i in %i", r, itemList.size());
#endif
	for (auto it : itemList) {
		if (++i == r) {
#ifdef _DEBUG
			MESSAGE("%s: we are returning %s %08X from the leveled list", __FUNCTION__, GetFullName(it.first), it.second);
#ifdef TRACE
			TRACEMESSAGE(lev,"End  : %s", __func__);
#endif
#endif
			return it;
		}
	}
#ifdef _DEBUG
	TRACEMESSAGE(lev,"End  : %s final null", __func__);
#endif
	return std::make_pair((TESForm*)NULL, (int)0);
}

bool getInventoryFromTESContainer(TESContainer* container, std::map<TESForm*, int>& itemList, bool addQuestItems) {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	bool hasFlag = false;
	if (container == nullptr)
	{
#ifdef TRACE
		TRACEMESSAGE("End  : %s container null", __func__);
#endif
		return false;
	}

	auto data = container->list.Info();
	if (data == nullptr)
	{
#ifdef TRACE
		TRACEMESSAGE("End  : %s container list info null", __func__);
#endif
		return false;
	}

	auto next = container->list.Next();
	while (data != NULL) {
		if (data->type->GetFormType() == kFormType_LeveledItem) {
			auto it = getFormFromTESLevItem(OBLIVION_CAST(data->type, TESForm, TESLevItem), addQuestItems);
			if (it.first != NULL) {
				itemList.insert(it);
			}
		}
		else {
			if (data->type == obrnFlag) {
#ifdef _DEBUG
				MESSAGE("%s: OBRN Flag has been found for", __FUNCTION__);
#endif
				hasFlag = true;
			}
			else if (!isQuestItem(data->type) || addQuestItems) {
				auto it = itemList.find(data->type);
				SInt32 count = 1 > data->count ? 1 : data->count;
				if (it == itemList.end()) {
					itemList.insert(std::make_pair(data->type, count));
				}
				else {
					it->second += count;
				}
			}
		}
		if (next == NULL) {
			break;
		}
		data = next->Info();
		next = next->Next();
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
	return hasFlag;
}

bool getContainerInventory(TESObjectREFR* ref, std::map<TESForm*, int> & itemList, bool addQuestItems) {
#ifdef TRACE
	TRACEMESSAGE(ref,"Start: %s", __func__);
#endif
	bool hasFlag = false;
	ExtraContainerChanges* cont = (ExtraContainerChanges*)ref->baseExtraList.GetByType(kExtraData_ContainerChanges);
	while (cont != NULL && cont->data != NULL && cont->data->objList != NULL) {
		for (auto it = cont->data->objList->Begin(); !it.End(); ++it) {
			TESForm* item = it->type;
			UInt32 count = 1 > it->countDelta ? 1 : it->countDelta; // max(1, it->countDelta); //certain items, for whatever reasons, have count 0
			if (item == obrnFlag) {
				hasFlag = true;
#ifdef _DEBUG
				MESSAGE("%s: OBRN Flag has been found for %s (%08X)", __FUNCTION__, GetFullName(ref), ref->refID);
#endif
				continue;
			}
			//MESSAGE("Ref: %s (%08X) (base: %s %08X) : found a %s (%08X %s), quantity: %i",
			//	GetFullName(ref), ref->refID, GetFullName(ref->baseForm), ref->baseForm->refID, GetFullName(item), item->refID, FormToString(item->GetFormType()), count);
			TESScriptableForm* scriptForm = OBLIVION_CAST(item, TESForm, TESScriptableForm);
			if (GetFullName(item)[0] != '<' && (addQuestItems || (!item->IsQuestItem() && (!oExcludeQuestItems || scriptForm == NULL || scriptForm->script == NULL)))) {
				auto it = itemList.find(item);
				if (it == itemList.end()) {
					itemList.insert(std::make_pair(item, count));
				}
				else {
					it->second += count;
				}
			}
		}
		BSExtraData* ptr = cont->next;
		while (ptr != NULL) {// && cont->next->type == kExtraData_ContainerChanges) {
			if (ptr->type == kExtraData_ContainerChanges) {
				break;
			}
			ptr = ptr->next;
		}
		cont = (ExtraContainerChanges*)ptr;
	}
	if (ref->GetFormType() == kFormType_ACRE && !itemList.size()) {
		TESActorBase* actorBase = OBLIVION_CAST(ref->baseForm, TESForm, TESActorBase);
		if (actorBase == NULL) {
#ifdef TRACE
			TRACEMESSAGE(ref,"End  : %s actorbase is null", __func__);
#endif
			return false;
		}
#ifdef TRACE
		TRACEMESSAGE(ref,"End  : %s" , __func__);
#endif
		return getInventoryFromTESContainer(&actorBase->container, itemList, addQuestItems);
	}
#ifdef TRACE
	TRACEMESSAGE(ref,"End  : %s final return", __func__);
#endif
	return hasFlag;
}

int getPlayerLevel() {
#ifdef TRACE
	TRACEMESSAGE("Start: %s", __func__);
#endif
	TESActorBase* actorBase = OBLIVION_CAST(*g_thePlayer, PlayerCharacter, TESActorBase);
	if (actorBase == NULL) {
#ifdef TRACE
		TRACEMESSAGE("End  : %s actorbase is null", __func__);
#endif
		return 0;
	}
#ifdef TRACE
	TRACEMESSAGE("End  : %s", __func__);
#endif
	return actorBase->actorBaseData.level;
}

int getRefLevelAdjusted(TESObjectREFR* ref) {
#ifdef TRACE
	TRACEMESSAGE(ref,"Start: %s", __func__);
#endif
	if (!ref->IsActor()) {
#ifdef TRACE
		TRACEMESSAGE(ref,"End  : %s ref is not an actor", __func__);
#endif
		return 1;
	}
	Actor* actor = OBLIVION_CAST(ref, TESObjectREFR, Actor);
	TESActorBase* actorBase = OBLIVION_CAST(actor->baseForm, TESForm, TESActorBase);
	//return min(1, actorBase->actorBaseData.IsPCLevelOffset() ? getPlayerLevel() + actorBase->actorBaseData.level : actorBase->actorBaseData.level);
	auto ret = actorBase->actorBaseData.IsPCLevelOffset() ? getPlayerLevel() + actorBase->actorBaseData.level : actorBase->actorBaseData.level;
#ifdef TRACE
	TRACEMESSAGE(ref,"End  : %s", __func__);
#endif
	return 1 < ret ? 1 : ret;
}

void randomizeInventory(TESObjectREFR* ref) {
#ifdef TRACE
	TRACEMESSAGE(ref,"Start: %s", __func__);
#endif
	if (allGenericItems.size() == 0 || allWeapons.size() == 0 || allClothingAndArmor.size() == 0) {
#ifdef TRACE
		TRACEMESSAGE(ref,"End  : %s sizes all 0", __func__);
#endif
		return;
	}
	std::map<TESForm*, int> removeItems;
	bool hasFlag = getContainerInventory(ref, removeItems, !oExcludeQuestItems);
	if (ref->GetFormType() == kFormType_ACRE) {
		if (obrnFlag == NULL || hasFlag) {
#ifdef TRACE
			TRACEMESSAGE(ref,"End  : %s obrnFlag is null", __func__);
#endif
			return;
		}
#ifdef _DEBUG
		MESSAGE("%s %08X didn't have the flag - adding it now", GetFullName(ref), ref->refID);
#endif
		ref->AddItem(obrnFlag, NULL, 1);
	}
	for (auto it = removeItems.begin(); it != removeItems.end(); ++it) {
		TESForm* item = it->first;
		int cnt = it->second;
		if (oRandInventory == 1) {
			switch (item->refID) {
			case ITEM_GOLD:
				ref->AddItem(item, NULL, myrand(5, 60) * getRefLevelAdjusted(ref) - cnt);
				break;
			case ITEM_LOCKPICK:
			case ITEM_REPAIRHAMMER:
				ref->AddItem(item, NULL, myrand(1, 4) *  cnt);
				break;
			default:
			{
				UInt32 selection;
				if (getRandomByType(item, selection)) {
					TESForm* newItem = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("Replacing item %s %08X with %s %08X x%i", GetFullName(item), item->refID, GetFullName(newItem), newItem->refID, cnt);
#endif
					ref->AddItem(newItem, NULL, cnt);
					if (ref->GetFormType() == kFormType_ACHR && itemIsEquippable(newItem)) {
						ref->Equip(newItem, 1, NULL, 0);
					}
				}
			}
			}
		}
		if (item->GetFormType() != kFormType_Book && item->GetFormType() != kFormType_Key &&
			/*(item->GetModIndex() != 0xFF && !skipMod[item->GetModIndex()])*/ item->GetModIndex() != 0xFF && item->GetModIndex() != randId && //i honestly do not remember why i put the original check here
			(!oExcludeQuestItems || !isQuestItem(item))) {
			ref->RemoveItem(item, NULL, cnt, 0, 0, NULL, NULL, NULL, 0, 0);
		}
	}
	removeItems.clear();
	if (oRandInventory == 1) {
#ifdef TRACE
		TRACEMESSAGE(ref,"End  : %s orandInventory = 1", __func__);
#endif
		return;
	}
	//granting random items
	UInt32 selection;
	if (ref->GetFormType() == kFormType_ACHR) {
		int roll = rand() % 100, level = getRefLevelAdjusted(ref);
		//Gold
		if (roll < 50) {
#ifdef _DEBUG
			MESSAGE("GOLD: %s receiving gold", GetFullName(ref));
#endif
			ref->AddItem(LookupFormByID(ITEM_GOLD), NULL, myrand(5, 60) * level);
		}
		//Weapon Randomization
		int clothingStatus = OBRNRC_LOWER | OBRNRC_FOOT | OBRNRC_HAND | OBRNRC_UPPER;
		bool gotTwoHanded = false;
		bool gotBow = false;
		for (int i = 0; i < 2; ++i) {
			roll = myrand(0, weaponRanges[UNARMED]);
			int v;
			for (v = 0; v < WRANGE_MAX; ++v) {
				if (roll < weaponRanges[v]) {
					break;
				}
			}
			switch (v) {
			case BLADES:
				if (getRandomForKey(&allWeapons, TESObjectWEAP::kType_BladeOneHand, selection)) {
					TESObjectWEAP* wp = OBLIVION_CAST(LookupFormByID(selection), TESForm, TESObjectWEAP);
#ifdef _DEBUG
					MESSAGE("BLADE: %s receiving %s", GetFullName(ref), GetFullName(wp));
#endif
					ref->AddItem(wp, NULL, 1);
					if (!i) {
						ref->Equip(wp, 1, NULL, 0);
					}
					if (wp->type == TESObjectWEAP::kType_BladeTwoHand) {
						gotTwoHanded = true;
					}
				}
				break;
			case BLUNT:
				if (getRandomForKey(&allWeapons, TESObjectWEAP::kType_BluntOneHand, selection)) {
					TESObjectWEAP* wp = OBLIVION_CAST(LookupFormByID(selection), TESForm, TESObjectWEAP);
#ifdef _DEBUG
					MESSAGE("BLUNT: %s receiving %s", GetFullName(ref), GetFullName(wp));
#endif
					ref->AddItem(wp, NULL, 1);
					if (!i) {
						ref->Equip(wp, 1, NULL, 0);
					}
					if (wp->type == TESObjectWEAP::kType_BluntTwoHand) {
						gotTwoHanded = true;
					}
				}
				break;
			case STAVES:
				if (getRandomForKey(&allWeapons, TESObjectWEAP::kType_Staff, selection)) {
					TESObjectWEAP* wp = OBLIVION_CAST(LookupFormByID(selection), TESForm, TESObjectWEAP);
#ifdef _DEBUG
					MESSAGE("STAFF: %s receiving %s", GetFullName(ref), GetFullName(wp));
#endif
					ref->AddItem(wp, NULL, 1);
					if (!i) {
						ref->Equip(wp, 1, NULL, 0);
					}
					gotTwoHanded = true;
				}
			case BOWS:
				if (getRandomForKey(&allWeapons, TESObjectWEAP::kType_Bow, selection)) {
					TESObjectWEAP* wp = OBLIVION_CAST(LookupFormByID(selection), TESForm, TESObjectWEAP);
#ifdef _DEBUG
					MESSAGE("BOW: %s receiving %s", GetFullName(ref), GetFullName(wp));
#endif
					ref->AddItem(wp, NULL, 1);
					if (!i) {
						ref->Equip(wp, 1, NULL, 0);
					}
					gotTwoHanded = true;
					gotBow = true;
				}
			default:
#ifdef _DEBUG
				MESSAGE("UNARMED: %s is unarmed", GetFullName(ref));
#endif
				break;
			}
		}
		if (!gotBow) {
			if (!myrand(0, 5)) {
				if (getRandomForKey(&allWeapons, TESObjectWEAP::kType_Bow, selection)) {
					TESObjectWEAP* wp = OBLIVION_CAST(LookupFormByID(selection), TESForm, TESObjectWEAP);
#ifdef _DEBUG
					MESSAGE("BOW: %s receiving %s", GetFullName(ref), GetFullName(wp));
#endif
					ref->AddItem(wp, NULL, 1);
					ref->Equip(wp, 1, NULL, 0);
					gotTwoHanded = true;
					gotBow = true;
				}
			}
		}
		if (gotBow) {
			//give arrows
			for (int i = 0; i < 10; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Ammo, selection)) {
					TESForm* ammo = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("ARROW: %s receiving %s", GetFullName(ref), GetFullName(ammo));
#endif
					ref->AddItem(ammo, NULL, myrand(5, 30));
					if (!i) {
						ref->Equip(ammo, 1, NULL, 0);
					}
					if (myrand(0, i + 2)) {
						break;
					}
				}
			}
		}
		//Shield
		if (!gotTwoHanded && !myrand(0, 2) && getRandomForKey(&allClothingAndArmor, kSlot_Shield, selection)) {
			TESForm* shield = LookupFormByID(selection);
#ifdef _DEBUG
			MESSAGE("SHIELD: %s receiving %s", GetFullName(ref), GetFullName(shield));
#endif
			ref->AddItem(shield, NULL, 1);
			ref->Equip(shield, 1, NULL, 0);
		}
		//Clothing
		if (!myrand(0, 19)) {
			clothingStatus &= ~OBRNRC_UPPER;
		}
		if (!myrand(0, 15)) {
			clothingStatus &= ~OBRNRC_LOWER;
		}
		if (!myrand(0, 13)) {
			clothingStatus &= ~OBRNRC_FOOT;
		}
		if (!myrand(0, 9)) {
			clothingStatus &= ~OBRNRC_HAND;
		}
		if (clothingStatus & OBRNRC_UPPER) {
			roll = myrand(0, clothingRanges[UPPERLOWERHAND]);
			int v;
			for (v = 0; v < CRANGE_MAX; ++v) {
				if (roll < clothingRanges[v]) {
					break;
				}
			}
			switch (v) {
			case UPPER:
				break;
			case UPPERHAND:
				clothingStatus &= ~OBRNRC_HAND;
				break;
			case UPPERLOWER:
				clothingStatus &= ~OBRNRC_LOWER;
				break;
			case UPPERLOWERFOOT:
				clothingStatus &= ~(OBRNRC_LOWER | OBRNRC_FOOT);
				break;
			case UPPERLOWERFOOTHAND:
				clothingStatus &= ~(OBRNRC_LOWER | OBRNRC_FOOT | OBRNRC_HAND);
				break;
			case UPPERLOWERHAND:
				clothingStatus &= ~(OBRNRC_LOWER | OBRNRC_HAND);
				break;
			default:
				break;
			}
			if (v != CRANGE_MAX) {
				if (getRandomForKey(&allClothingAndArmor, clothingKeys[v], selection)) {
					TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("UPPER: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
					ref->AddItem(cloth, NULL, 1);
					ref->Equip(cloth, 1, NULL, 0);
				}
			}
			else {
#ifdef _DEBUG
				_ERROR("v == CRANGE_MAX. this should not happen");
#endif
			}
		}
		if (clothingStatus & OBRNRC_LOWER) {
			if (getRandomForKey(&allClothingAndArmor, kSlot_LowerBody, selection)) {
				TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("LOWER: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
				ref->AddItem(cloth, NULL, 1);
				ref->Equip(cloth, 1, NULL, 0);
			}
		}
		if (clothingStatus & OBRNRC_FOOT) {
			if (getRandomForKey(&allClothingAndArmor, kSlot_Foot, selection)) {
				TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("FOOT: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
				ref->AddItem(cloth, NULL, 1);
				ref->Equip(cloth, 1, NULL, 0);
			}
		}
		if (clothingStatus & OBRNRC_HAND) {
			if (getRandomForKey(&allClothingAndArmor, kSlot_Hand, selection)) {
				TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("HAND: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
				ref->AddItem(cloth, NULL, 1);
				ref->Equip(cloth, 1, NULL, 0);
			}
		}
		//Head
		if (myrand(0, 2)) {
			if (getRandomForKey(&allClothingAndArmor, kSlot_Head, selection)) {
				TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("HEAD: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
				ref->AddItem(cloth, NULL, 1);
				ref->Equip(cloth, 1, NULL, 0);
			}
		}
		//Rings
		for (int i = 0; i < 2; ++i) {
			if (!myrand(0, i + 1)) {
				if (getRandomForKey(&allClothingAndArmor, kSlot_LeftRing, selection)) {
					TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("RING: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
					ref->AddItem(cloth, NULL, 1);
					ref->Equip(cloth, 1, NULL, 0);
				}
			}
		}
		//Amulets
		if (!myrand(0, 2)) {
			if (getRandomForKey(&allClothingAndArmor, kSlot_Amulet, selection)) {
				TESForm* cloth = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("AMULET: %s receiving %s", GetFullName(ref), GetFullName(cloth));
#endif
				ref->AddItem(cloth, NULL, 1);
				ref->Equip(cloth, 1, NULL, 0);
			}
		}
		//Potions
		for (int i = 0; i < 5; ++i) {
			if (myrand(0, 2)) {
				break;
			}
			if (getRandomForKey(&allGenericItems, kFormType_AlchemyItem, selection)) {
				TESForm* item = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("POTION: %s receiving %s", GetFullName(ref), GetFullName(item));
#endif
				ref->AddItem(item, NULL, myrand(1, 4));
			}
		}
		//Soul Gems
		for (int i = 0; i < 6; ++i) {
			if (myrand(0, 4)) {
				break;
			}
			if (getRandomForKey(&allGenericItems, kFormType_SoulGem, selection)) {
				TESForm* item = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("SOULGEM: %s receiving %s", GetFullName(ref), GetFullName(item));
#endif
				ref->AddItem(item, NULL, myrand(1, 2));
			}
		}
		//Sigil Stones
		if (!myrand(0, 9)) {
			if (getRandomForKey(&allGenericItems, kFormType_SigilStone, selection)) {
				TESForm* item = LookupFormByID(selection);
#ifdef _DEBUG
				MESSAGE("SIGILSTONE: %s receiving %s", GetFullName(ref), GetFullName(item));
#endif
				ref->AddItem(item, NULL, 1);
			}
		}
		//Ingredients
		if (!myrand(0, 5)) {
			for (int i = 0; i < 10; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Ingredient, selection)) {
					TESForm* item = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("INGREDIENT: %s receiving %s", GetFullName(ref), GetFullName(item));
#endif
					ref->AddItem(item, NULL, myrand(1, 6));
				}
				if (myrand(0, 3)) {
					break;
				}
			}
		}
		//Apparatus
		if (!myrand(0, 9)) {
			for (int i = 0; i < 3; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Apparatus, selection)) {
					TESForm* item = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("APPARATUS: %s receiving %s", GetFullName(ref), GetFullName(item));
#endif
					ref->AddItem(item, NULL, myrand(1, 2));
				}
				if (myrand(0, 2)) {
					break;
				}
			}
		}
		//Clutter
		if (!myrand(0, 9)) {
			for (int i = 0; i < 9; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Misc, selection)) {
					TESForm* item = LookupFormByID(selection);
#ifdef _DEBUG
					MESSAGE("GENERIC: %s receiving %s", GetFullName(ref), GetFullName(item));
#endif
					ref->AddItem(item, NULL, myrand(1, 3));
				}
				if (myrand(0, i + 1)) {
					break;
				}
			}
		}
	}
	else {//containers
		//Gold
		if (!myrand(0, 2)) {
			ref->AddItem(LookupFormByID(ITEM_GOLD), NULL, myrand(1, 100) * (!myrand(0, 4) ? 30 : 1));
		}
		//Lockpick
		if (!myrand(0, 4)) {
			ref->AddItem(LookupFormByID(ITEM_LOCKPICK), NULL, myrand(1, 10));
		}
		//Repair Hammer
		if (!myrand(0, 4)) {
			ref->AddItem(LookupFormByID(ITEM_REPAIRHAMMER), NULL, myrand(1, 3));
		}
		//Clutter
		if (!myrand(0, 5)) {
			for (int i = 0; i < 3; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Misc, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, myrand(1, 5));
					if (myrand(0, i + 3)) {
						break;
					}
				}
			}
		}
		//Potions
		if (!myrand(0, 3)) {
			for (int i = 0; i < 5; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_AlchemyItem, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, myrand(1, 5));
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Ingredients
		if (!myrand(0, 4)) {
			for (int i = 0; i < 6; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Ingredient, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, myrand(1, 2));
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Books
		if (!myrand(0, 4)) {
			for (int i = 0; i < 3; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Book, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, 1);
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Soul Gems
		if (!myrand(0, 9)) {
			for (int i = 0; i < 3; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_SoulGem, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, myrand(1, 3));
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Sigil Stones
		if (!myrand(0, 11)) {
			for (int i = 0; i < 2; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_SigilStone, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, 1);
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Keys
		if (!myrand(0, 6)) {
			if (getRandomForKey(&allGenericItems, kFormType_Key, selection)) {
				ref->AddItem(LookupFormByID(selection), NULL, 1);
			}
		}
		//Apparatus
		if (!myrand(0, 8)) {
			for (int i = 0; i < 4; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Apparatus, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, 1);
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Weapons
		if (!myrand(0, 4)) {
			for (int i = 0; i < 3; ++i) {
				int wpType = -1;
				switch (myrand(0, 3)) {
				case 0:
					wpType = TESObjectWEAP::kType_BladeOneHand;
					break;
				case 1:
					wpType = TESObjectWEAP::kType_BluntOneHand;
					break;
				case 2:
					wpType = TESObjectWEAP::kType_Bow;
					break;
				default:
					wpType = TESObjectWEAP::kType_Staff;
					break;
				}
				if (getRandomForKey(&allWeapons, wpType, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, 1);
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Ammo
		if (!myrand(0, 4)) {
			for (int i = 0; i < 4; ++i) {
				if (getRandomForKey(&allGenericItems, kFormType_Ammo, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, myrand(1, 30));
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
		//Clothing / Armor
		if (!myrand(0, 2)) {
			for (int i = 0; i < 7; ++i) {
				UInt32 key = 0xFFFFFFFF;
				switch (myrand(0, 12)) { //was 0, 7 in the script
				case 0:
					key = kSlot_Amulet;
					break;
				case 1:
					key = kSlot_Foot;
					break;
				case 2:
					key = kSlot_Hand;
					break;
				case 3:
					key = kSlot_Head;
					break;
				case 4:
					key = kSlot_LowerBody;
					break;
				case 5:
					key = kSlot_LeftRing;
					break;
				case 6:
					key = kSlot_Shield;
					break;
				case 7:
					key = kSlot_UpperBody;
					break;
				case 8:
					key = kSlot_UpperHand;
					break;
				case 9:
					key = kSlot_UpperLower;
					break;
				case 10:
					key = kSlot_UpperLowerFoot;
					break;
				case 11:
					key = kSlot_UpperLowerHandFoot;
					break;
				default:
					key = kSlot_UpperLowerHand;
					break;
				}
				if (key != 0xFFFFFFFF && getRandomForKey(&allClothingAndArmor, key, selection)) {
					ref->AddItem(LookupFormByID(selection), NULL, 1);
					if (myrand(0, i + 1)) {
						break;
					}
				}
			}
		}
	}
#ifdef TRACE
	TRACEMESSAGE(ref,"End  : %s", __func__);
#endif
}

TESForm* getFormFromLeveledList(TESLevItem* lev) {
#ifdef TRACE
	TRACEMESSAGE(lev,"Start: %s", __func__);
#endif
	if (lev == NULL) {
#ifdef TRACE
		TRACEMESSAGE(lev, "End  : %s lev is null", __func__);
#endif
		return NULL;
	}
	std::map<UInt32, TESForm*> forms;
	if (!getFormsFromLeveledList(lev, forms) || !forms.size()) {
#ifdef TRACE
		TRACEMESSAGE(lev, "End  : %s forms is null", __func__);
#endif
		return NULL;
	}
	int i = -1, cnt = myrand(0, forms.size() - 1);
	auto it = forms.begin();
	while (it != forms.end()) {
		if (++i == cnt) {
#ifdef TRACE
			TRACEMESSAGE(lev, "End: %s", __func__);
#endif
			return it->second;
		}
		++it;
	}
#ifdef TRACE
	TRACEMESSAGE(lev, "End  : %s return null", __func__);
#endif
	return NULL;
}

bool getRandom(TESForm* f, UInt32& out) {
#ifdef TRACE
	TRACEMESSAGE(f,"Start: %s", __func__);
#endif
	if (!allItems.size()) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s allitems is empty", __func__);
#endif
		return false;
	}
	if (f == NULL) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s tesform f is null", __func__);
#endif
		return false;
	}
	if (oExcludeQuestItems && isQuestItem(f)) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s Quest items excluded ", __func__);
#endif
		return false;
	}
	if (f->GetModIndex() != 0xFF && f->GetModIndex() == randId) {
		//if (f->GetModIndex() != 0xFF && skipMod[f->GetModIndex()]) { //very important!
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s f->getmodindex at 255 or randID - randID = %i", __func__, randId);
#endif
		return false;
	}
	out = allItems[rand() % allItems.size()];
#ifdef TRACE
	TRACEMESSAGE(f,"End  : %s", __func__);
#endif
	return true;
}

bool getRandomByType(TESForm *f, UInt32& out) {
#ifdef TRACE
	TRACEMESSAGE(f,"Start: %s", __func__);
#endif
	ItemMapPtr ptr = NULL;
	UInt32 key = 0xFFFFFFFF;
	if (f == NULL) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s - tesform f is null", __func__);
#endif
		return false;
	}
	if (oExcludeQuestItems && isQuestItem(f)) {
#ifdef TRACE
		TRACEMESSAGE(f,"End  : %s quest items excluded", __func__);
#endif
		return false;
	}
	if (f->GetModIndex() != 0xFF && f->GetModIndex() == randId) {
	//if (f->GetModIndex() != 0xFF && skipMod[f->GetModIndex()]) { //very important!
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s f->getmodIndex is 255 or randid, randid = %i", __func__, randId);
#endif
		return false;
	}
	switch (f->GetFormType()) {
	case kFormType_LeveledItem:
	{
		TESLevItem* lev = OBLIVION_CAST(f, TESForm, TESLevItem);
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s lev = OBLIVION_CAST(f, TESForm, TESLevItem)", __func__);
#endif
		return getRandomByType(getFormFromLeveledList(lev), out);
	}
	case kFormType_Armor:
	case kFormType_Clothing:
	{
		ptr = &allClothingAndArmor;
		if (f->GetFormType() == kFormType_Armor) {
			TESObjectARMO* armor = OBLIVION_CAST(f, TESForm, TESObjectARMO);
			key = armor->bipedModel.partMask & ~kSlot_Tail; //yeah... we kinda dont care about this slot
		}
		else {
			TESObjectCLOT* clothing = OBLIVION_CAST(f, TESForm, TESObjectCLOT);
			key = clothing->bipedModel.partMask;
		}
		if (key & kSlot_RightRing) {
			key = kSlot_LeftRing;
		}
		if (key & kSlot_Hair || key & kSlot_Head) {
			key = kSlot_Head;
		}
		break;
	}
	case kFormType_Weapon:
	{
		ptr = &allWeapons;
		TESObjectWEAP* weapon = OBLIVION_CAST(f, TESForm, TESObjectWEAP);
		key = weapon->type;
		if (key == TESObjectWEAP::kType_BladeTwoHand) {
			key = TESObjectWEAP::kType_BladeOneHand;
		}
		else if (key == TESObjectWEAP::kType_BluntTwoHand) {
			key = TESObjectWEAP::kType_BluntOneHand;
		}
		break;
	}
	case kFormType_Apparatus:
	case kFormType_Book:
	case kFormType_Ingredient:
	case kFormType_Misc:
	case kFormType_Ammo:
	case kFormType_SoulGem:
	case kFormType_Key:
	case kFormType_AlchemyItem:
	case kFormType_SigilStone:
		ptr = &allGenericItems;
		key = f->GetFormType();
	default:
		break;
	}
	if (ptr != NULL && key != 0xFFFFFFFF) {
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s", __func__);
#endif
		return getRandomForKey(ptr, key, out);
	}
#ifdef TRACE
	TRACEMESSAGE(f, "End  : %s return false", __func__);
#endif
	return false;
}

bool getRandomBySetting(TESForm* f, UInt32& out, int option) {
#ifdef TRACE
	TRACEMESSAGE(f, "Start: %s", __func__);
#endif
	switch (option) {
	case 0:
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s case 0", __func__);
#endif
		return false;
	case 1:
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s case 1", __func__);
#endif
		return getRandomByType(f, out);
	case 2:
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s case 2", __func__);
#endif
		return getRandom(f, out);
	default:
		MESSAGE("Invalid option %i for getRandomBySetting", option);
#ifdef TRACE
		TRACEMESSAGE(f, "End  : %s invalid setting", __func__);
#endif
		return false;
	}
}

void randomize(TESObjectREFR* ref, const char* function) {
#ifdef TRACE
	TRACEMESSAGE(ref, "Start: %s", __func__);
#endif
#ifdef _DEBUG
	MESSAGE("%s: Attempting to randomize %s %08X", function, GetFullName(ref), ref->refID);
#endif
	if (ref->GetFormType() == kFormType_ACRE) {
		if (allCreatures.size() == 0) {
#ifdef TRACE
			TRACEMESSAGE(ref, "End  : %s allcreatures is empty", __func__);
#endif
			return;
		}
		if (strcmp(function, "ESP") == 0) {
			QueueUIMessage_2("Randomizing creatures through the spell may cause issues", 1000, NULL, NULL);
		}
		else if (!oRandCreatures) {
#ifdef TRACE
			TRACEMESSAGE(ref, "End  : %s oRandCreatures is 0", __func__);
#endif
			return;
		}
		Actor* actor = OBLIVION_CAST(ref, TESObjectREFR, Actor);
		if (actor == NULL) {
#ifdef TRACE
			TRACEMESSAGE(ref, "End  : %s actor = null", __func__);
#endif
			return;
		}
		UInt32 health = actor->GetBaseActorValue(kActorVal_Health), aggression = actor->GetActorValue(kActorVal_Aggression);//actor->GetBaseActorValue(kActorVal_Aggression);
		if (health == 0) {
			if (loading_game) {
				toRandomize.push_back(ref);
#ifdef TRACE
				TRACEMESSAGE(ref, "End  : %s game loading", __func__);
#endif
				return;
			}
#ifdef _DEBUG
			MESSAGE("%s: Dead creature %s %08X will be treated as a container", function, GetFullName(ref), ref->refID);
#endif
			randomizeInventory(ref);
#ifdef TRACE
			TRACEMESSAGE(ref, "End  : %s dead creature treated as container", __func__);
#endif
			return;
		}
		std::map<TESForm*, int> keepItems;
		getContainerInventory(ref, keepItems, true);
		TESForm* oldBaseForm = ref->GetTemplateForm() != NULL ? ref->GetTemplateForm() : ref->baseForm,
			* rando = LookupFormByID(allCreatures[rand() % allCreatures.size()]);
		TESCreature* creature = OBLIVION_CAST(rando, TESForm, TESCreature);
		if (creature != NULL) {
			getInventoryFromTESContainer(&creature->container, keepItems, true);
		}
#ifdef _DEBUG
		MESSAGE("%s: Going to randomize %s %08X into %s %08X", function, GetFullName(ref), ref->refID, GetFullName(rando), rando->refID);
#endif
		//TESActorBase* actorBase = OBLIVION_CAST(rando, TESForm, TESActorBase);
		ref->baseForm = rando;
		ref->SetTemplateForm(oldBaseForm);
		actor->SetActorValue(kActorVal_Aggression, aggression);
		for (auto it = keepItems.begin(); it != keepItems.end(); ++it) {
			TESForm* item = it->first;
			int cnt = it->second;
			ref->AddItem(item, NULL, cnt);
		}
	}
	else if (refIsItem(ref)) {
		if (!oWorldItems) {
#ifdef TRACE
			TRACEMESSAGE(ref, "End: %s oWorldItems is 0", __func__);
#endif
			return;
		}
#ifdef _DEBUG
		MESSAGE("%s: World item randomization: will try to randomize %s %08X", function, GetFullName(ref), ref->refID);
#endif
		UInt32 selection;
		if (getRandomBySetting(ref->baseForm, selection, oWorldItems)) {
			TESForm* rando = LookupFormByID(selection);
#ifdef _DEBUG
			MESSAGE("%s: Going to randomize %s %08X into %s %08X", function, GetFullName(ref), ref->refID, GetFullName(rando), rando->refID);
#endif
			ref->baseForm = rando;
			ref->Update3D();
		}
	}
	else {
		randomizeInventory(ref);
	}
#ifdef TRACE
	TRACEMESSAGE(ref, "End  : %s", __func__);
#endif
}