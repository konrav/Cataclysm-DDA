#include "game.h"
#include "options.h"
#include "output.h"
#include "debug.h"
#include "translations.h"
#include "file_finder.h"
#include "cursesdef.h"
#include "path_info.h"
#include "mapsharing.h"
#include "file_wrapper.h"

#ifdef SDLTILES
#include "cata_tiles.h"
#endif // SDLTILES

#include <stdlib.h>
#include <fstream>
#include <string>
#include <locale>
#include <sstream>

bool trigdist;
bool use_tiles;

bool used_tiles_changed;
#ifdef SDLTILES
extern cata_tiles *tilecontext;
#endif // SDLTILES

std::unordered_map<std::string, cOpt> OPTIONS;
std::unordered_map<std::string, cOpt> ACTIVE_WORLD_OPTIONS;
options_data optionsdata; // store extranious options data that doesn't need to be in OPTIONS,
std::vector<std::pair<std::string, std::string> > vPages;
std::map<int, std::vector<std::string> > mPageItems;
std::map<std::string, int> mOptionsSort;
std::map<std::string, std::string> optionNames;
int iWorldOptPage;

options_data::options_data()
{
    enable_json("DEFAULT_REGION");
    // to allow class based init_data functions to add values to a 'string' type option, add:
    //   enable_json("OPTION_KEY_THAT_GETS_STRING_ENTRIES_ADDED_VIA_JSON");
    // also, in options.h, add this before 'class options_data'
    //   class my_class;
    // and inside options_data above public:
    //   friend class my_class;
    // then, in the my_class::load_json (or post-json setup) method:
    //   optionsdata.addme("OPTION_KEY_THAT_GETS_STRING_ENTRIES_ADDED_VIA_JSON", "thisvalue");
}

//Default constructor
cOpt::cOpt()
{
    sType = "VOID";
    sPage = "";
}

//string constructor
cOpt::cOpt(const std::string sPageIn, const std::string sMenuTextIn, const std::string sTooltipIn,
           const std::string sItemsIn, std::string sDefaultIn, copt_hide_t opt_hide=COPT_NO_HIDE)
{
    sPage = sPageIn;
    sMenuText = sMenuTextIn;
    sTooltip = sTooltipIn;
    sType = "string";

    hide = opt_hide;

    std::stringstream ssTemp(sItemsIn);
    std::string sItem;
    while (std::getline(ssTemp, sItem, ',')) {
        vItems.push_back(sItem);
    }

    if (getItemPos(sDefaultIn) == -1) {
        sDefaultIn = vItems[0];
    }

    sDefault = sDefaultIn;
    sSet = sDefaultIn;

    setSortPos(sPageIn);
}

//bool constructor
cOpt::cOpt(const std::string sPageIn, const std::string sMenuTextIn, const std::string sTooltipIn,
           const bool bDefaultIn, copt_hide_t opt_hide=COPT_NO_HIDE)
{
    sPage = sPageIn;
    sMenuText = sMenuTextIn;
    sTooltip = sTooltipIn;
    sType = "bool";

    hide = opt_hide;

    bDefault = bDefaultIn;
    bSet = bDefaultIn;

    setSortPos(sPageIn);
}

//int constructor
cOpt::cOpt(const std::string sPageIn, const std::string sMenuTextIn, const std::string sTooltipIn,
           const int iMinIn, int iMaxIn, int iDefaultIn, copt_hide_t opt_hide=COPT_NO_HIDE)
{
    sPage = sPageIn;
    sMenuText = sMenuTextIn;
    sTooltip = sTooltipIn;
    sType = "int";

    hide = opt_hide;

    if (iMinIn > iMaxIn) {
        iMaxIn = iMinIn;
    }

    iMin = iMinIn;
    iMax = iMaxIn;

    if (iDefaultIn < iMinIn || iDefaultIn > iMaxIn) {
        iDefaultIn = iMinIn ;
    }

    iDefault = iDefaultIn;
    iSet = iDefaultIn;

    setSortPos(sPageIn);
}

//float constructor
cOpt::cOpt(const std::string sPageIn, const std::string sMenuTextIn, const std::string sTooltipIn,
           const float fMinIn, float fMaxIn, float fDefaultIn, float fStepIn, copt_hide_t opt_hide=COPT_NO_HIDE)
{
    sPage = sPageIn;
    sMenuText = sMenuTextIn;
    sTooltip = sTooltipIn;
    sType = "float";

    hide = opt_hide;

    if (fMinIn > fMaxIn) {
        fMaxIn = fMinIn;
    }

    fMin = fMinIn;
    fMax = fMaxIn;
    fStep = fStepIn;

    if (fDefaultIn < fMinIn || fDefaultIn > fMaxIn) {
        fDefaultIn = fMinIn ;
    }

    fDefault = fDefaultIn;
    fSet = fDefaultIn;

    setSortPos(sPageIn);
}

//helper functions
bool cOpt::is_hidden()
{
    switch(hide)
    {
        case COPT_NO_HIDE:
            return false;

        case COPT_SDL_HIDE:
#ifdef SDLTILES
            return true;
#else
            return false;
#endif

        case COPT_CURSES_HIDE:
#ifndef SDLTILES // If not defined. it's curses interface.
            return true;
#else
            return false;
#endif

        default:
            return false; // No hide on default
    }
}

void cOpt::setSortPos(const std::string sPageIn)
{
    mOptionsSort[sPageIn]++;
    iSortPos = mOptionsSort[sPageIn] - 1;
}

int cOpt::getSortPos()
{
    return iSortPos;
}

std::string cOpt::getPage()
{
    return sPage;
}

std::string cOpt::getMenuText()
{
    return sMenuText;
}

std::string cOpt::getTooltip()
{
    return sTooltip;
}

std::string cOpt::getType()
{
    return sType;
}

std::string cOpt::getValue()
{
    if (sType == "string") {
        return sSet;

    } else if (sType == "bool") {
        return (bSet) ? "true" : "false";

    } else if (sType == "int") {
        std::stringstream ssTemp;
        ssTemp << iSet;
        return ssTemp.str();

    } else if (sType == "float") {
        std::stringstream ssTemp;
        ssTemp.imbue(std::locale("C"));
        const int precision = (fStep >= 0.09) ? 1 : (fStep >= 0.009) ? 2 : (fStep >= 0.0009) ? 3 : 4;
        ssTemp.precision(precision);
        ssTemp << std::fixed << fSet;
        return ssTemp.str();
    }

    return "";
}

std::string cOpt::getValueName()
{
    if (sType == "string") {
        return optionNames[sSet];

    } else if (sType == "bool") {
        return (bSet) ? _("True") : _("False");
    }

    return getValue();
}

std::string cOpt::getDefaultText(const bool bTranslated)
{
    if (sType == "string") {
        std::string sItems = "";
        for (size_t i = 0; i < vItems.size(); i++) {
            if (sItems != "") {
                sItems += _(", ");
            }
            sItems += (bTranslated) ? optionNames[vItems[i]] : vItems[i];
        }
        return string_format(_("Default: %s - Values: %s"), (bTranslated) ? optionNames[sDefault].c_str() : sDefault.c_str(), sItems.c_str());

    } else if (sType == "bool") {
        return (bDefault) ? _("Default: True") : _("Default: False");

    } else if (sType == "int") {
        return string_format(_("Default: %d - Min: %d, Max: %d"), iDefault, iMin, iMax);

    } else if (sType == "float") {
        return string_format(_("Default: %.2f - Min: %.2f, Max: %.2f"), fDefault, fMin, fMax);
    }

    return "";
}

int cOpt::getItemPos(const std::string sSearch)
{
    if (sType == "string") {
        for (size_t i = 0; i < vItems.size(); i++) {
            if (vItems[i] == sSearch) {
                return i;
            }
        }
    }

    return -1;
}

//set to next item
void cOpt::setNext()
{
    if (sType == "string") {
        int iNext = getItemPos(sSet) + 1;
        if (iNext >= vItems.size()) {
            iNext = 0;
        }

        sSet = vItems[iNext];

    } else if (sType == "bool") {
        bSet = !bSet;

    } else if (sType == "int") {
        iSet++;
        if (iSet > iMax) {
            iSet = iMin;
        }

    } else if (sType == "float") {
        fSet += fStep;
        if (fSet > fMax) {
            fSet = fMin;
        }
    }
}

//set to prev item
void cOpt::setPrev()
{
    if (sType == "string") {
        int iPrev = getItemPos(sSet) - 1;
        if (iPrev < 0) {
            iPrev = vItems.size() - 1;
        }

        sSet = vItems[iPrev];

    } else if (sType == "bool") {
        bSet = !bSet;

    } else if (sType == "int") {
        iSet--;
        if (iSet < iMin) {
            iSet = iMax;
        }

    } else if (sType == "float") {
        fSet -= fStep;
        if (fSet < fMin) {
            fSet = fMax;
        }
    }
}

//set value
void cOpt::setValue(float fSetIn)
{
    if (sType != "float") {
        debugmsg("tried to set a float value to a %s option", sType.c_str());
        return;
    }
    fSet = fSetIn;
    if ( fSet < fMin || fSet > fMax ) {
        fSet = fDefault;
    }
}

//set value
void cOpt::setValue(std::string sSetIn)
{
    if (sType == "string") {
        if (getItemPos(sSetIn) != -1) {
            sSet = sSetIn;
        }

    } else if (sType == "bool") {
        bSet = (sSetIn == "True" || sSetIn == "true" || sSetIn == "T" || sSetIn == "t");

    } else if (sType == "int") {
        iSet = atoi(sSetIn.c_str());

        if ( iSet < iMin || iSet > iMax ) {
            iSet = iDefault;
        }

    } else if (sType == "float") {
        std::istringstream ssTemp(sSetIn);
        ssTemp.imbue(std::locale("C"));
        float tmpFloat;
        ssTemp >> tmpFloat;
        if(ssTemp) {
            setValue(tmpFloat);
        } else {
            debugmsg("invalid floating point option: %s", sSetIn.c_str());
        }
    }
}

//Set default class behaviour to float
cOpt::operator float() const
{
    if (sType == "string") {
        return (sSet != "" && sSet == sDefault) ? 1.0 : 0.0;

    } else if (sType == "bool") {
        return (bSet) ? 1.0 : 0.0;

    } else if (sType == "int") {
        return (float)iSet;

    } else if (sType == "float") {
        return fSet;
    }

    return 0.0;
}

// if (class == "string")
bool cOpt::operator==(const std::string sCompare) const
{
    return (sType == "string" && sSet == sCompare);
}

// if (class != "string")
bool cOpt::operator!=(const std::string sCompare) const
{
    return !(*this == sCompare);
}

void initOptions()
{
    vPages.clear();
    vPages.push_back(std::make_pair("general", _("General")));
    vPages.push_back(std::make_pair("interface", _("Interface")));
    vPages.push_back(std::make_pair("graphics", _("Graphics")));
    // when sharing maps only admin is allowed to change these.
    if(!MAP_SHARING::isCompetitive() || MAP_SHARING::isAdmin()) {
        vPages.push_back(std::make_pair("debug", _("Debug")));
    }
    iWorldOptPage = vPages.size();
    // when sharing maps only admin is allowed to change these.
    if(!MAP_SHARING::isCompetitive() || MAP_SHARING::isAdmin()) {
        vPages.push_back(std::make_pair("world_default", _("World Defaults")));
    }

    OPTIONS.clear();

    std::string tileset_names;
    tileset_names = get_tileset_names(FILENAMES["gfxdir"]); //get the tileset names and set the optionNames

    ////////////////////////////GENERAL//////////////////////////
    OPTIONS["AUTO_PICKUP"] = cOpt("general", _("Auto pickup enabled"),
                                  _("Enable item auto pickup. Change pickup rules with the Auto Pickup Manager in the Help Menu ?3"),
                                  false
                                 );

    OPTIONS["AUTO_PICKUP_ADJACENT"] = cOpt("general", _("Auto pickup adjacent"),
                                           _("If true will enable to pickup items one tile around to the player. You can assign No Auto Pikcup zones with the Zones Manager 'Y' key for eg. your homebase."),
                                           false
                                          );

    OPTIONS["AUTO_PICKUP_ZERO"] = cOpt("general", _("Auto pickup 0 vol light items"),
                                       _("Auto pickup items with 0 Volume, and weight less than or equal to [option] * 50 grams. '0' disables this option"),
                                       0, 20, 0
                                      );

    OPTIONS["AUTO_PICKUP_SAFEMODE"] = cOpt("general", _("Auto pickup safemode"),
                                           _("Auto pickup is disabled as long as you can see monsters nearby. This is affected by Safemode proximity distance."),
                                           false
                                          );

    mOptionsSort["general"]++;

    OPTIONS["DANGEROUS_PICKUPS"] = cOpt("general", _("Dangerous pickups"),
                                        _("If false will cause player to drop new items that cause them to exceed the weight limit."),
                                        false
                                       );

    mOptionsSort["general"]++;

    OPTIONS["AUTOSAFEMODE"] = cOpt("general", _("Auto-safemode on by default"),
                                   _("If true, auto-safemode will be on after starting a new game or loading."),
                                   false
                                  );

    OPTIONS["AUTOSAFEMODETURNS"] = cOpt("general", _("Turns to reenable safemode"),
                                        _("Number of turns after safemode is reenabled if no hostiles are in safemodeproximity distance."),
                                        1, 100, 50
                                       );

    OPTIONS["SAFEMODE"] = cOpt("general", _("Safemode on by default"),
                               _("If true, safemode will be on after starting a new game or loading."),
                               true
                              );

    OPTIONS["SAFEMODEPROXIMITY"] = cOpt("general", _("Safemode proximity distance"),
                                        _("If safemode is enabled, distance to hostiles when safemode should show a warning. 0 = Max player viewdistance."),
                                        0, 50, 0
                                       );

    OPTIONS["SAFEMODEVEH"] = cOpt("general", _("Safemode when driving"),
                                  _("When true, safemode will alert you to hostiles whilst you are driving a vehicle."),
                                  false
                                 );

    mOptionsSort["general"]++;

    OPTIONS["AUTOSAVE"] = cOpt("general", _("Periodically autosave"),
                               _("If true, game will periodically save the map."),
                               false
                              );

    OPTIONS["AUTOSAVE_TURNS"] = cOpt("general", _("Game turns between autosaves"),
                                     _("Number of game turns between autosaves"),
                                     1, 1000, 5
                                    );

    OPTIONS["AUTOSAVE_MINUTES"] = cOpt("general", _("Real minutes between autosaves"),
                                       _("Number of real time minutes between autosaves"),
                                       0, 127, 5
                                      );

    mOptionsSort["general"]++;

    OPTIONS["CIRCLEDIST"] = cOpt("general", _("Circular distances"),
                                 _("If true, the game will calculate range in a realistic way: light sources will be circles diagonal movement will cover more ground and take longer. If disabled, everything is square: moving to the northwest corner of a building takes as long as moving to the north wall."),
                                 false
                                );

    optionNames["no"] = _("No");
    //~ containers
    optionNames["watertight"] = _("Watertight");
    optionNames["all"] = _("All");
    OPTIONS["DROP_EMPTY"] = cOpt("general", _("Drop empty containers"),
                                 _("Set to drop empty containers after use. No: Don't drop any. - Watertight: All except watertight containers. - All: Drop all containers."),
                                 "no,watertight,all", "no"
                                );

    OPTIONS["AUTO_NOTES"] = cOpt("general", _("Auto notes"),
                                 _("If true automatically sets notes on places that have stairs that go up or down"),
                                 false
                                );

    ////////////////////////////INTERFACE////////////////////////
    // TODO: scan for languages like we do for tilesets.
    optionNames[""] = _("System language");
    optionNames["cs"] = _("Czech");
    optionNames["en"] = _("English");
    optionNames["fr_FR"] = _("French");
    optionNames["de_DE"] = _("German");
    optionNames["it"] = _("Italian");
    optionNames["ja"] = _("Japanese");
    optionNames["ko"] = _("Korean");
    optionNames["pl"] = _("Polish");
    optionNames["pt_BR"] = _("Brazilian Portuguese");
    optionNames["pt_PT"] = _("Portuguese, Portugal");
    optionNames["ru"] = _("Russian");
    optionNames["sr"] = _("Serbian");
    optionNames["vi"] = _("Vietnamese");
    optionNames["zh_CN"] = _("Simplified Chinese");
    optionNames["zh_TW"] = _("Traditional Chinese");
    OPTIONS["USE_LANG"] = cOpt("interface", _("Language"), _("Switch Language. Requires restart."),
                               ",cs,en,fr_FR,de_DE,it,ja,ko,pl,pt_BR,pt_PT,ru,sr,vi,zh_CN,zh_TW",
                               ""
                              );

    mOptionsSort["interface"]++;

    optionNames["fahrenheit"] = _("Fahrenheit");
    optionNames["celsius"] = _("Celsius");
    OPTIONS["USE_CELSIUS"] = cOpt("interface", _("Temperature units"),
                                  _("Switch between Celsius and Fahrenheit."),
                                  "fahrenheit,celsius", "fahrenheit"
                                 );

    optionNames["mph"] = _("mph");
    optionNames["km/h"] = _("km/h");
    OPTIONS["USE_METRIC_SPEEDS"] = cOpt("interface", _("Speed units"),
                                        _("Switch between km/h and mph."),
                                        "mph,km/h", "mph"
                                       );

    optionNames["lbs"] = _("lbs");
    optionNames["kg"] = _("kg");
    OPTIONS["USE_METRIC_WEIGHTS"] = cOpt("interface", _("Mass units"),
                                         _("Switch between kg and lbs."),
                                         "lbs,kg", "lbs"
                                        );

    //~ 12h time, e.g. 11:59pm
    optionNames["12h"] = _("12h");
    //~ Military time, e.g. 2359
    optionNames["military"] = _("Military");
    //~ 24h time, e.g. 23:59
    optionNames["24h"] = _("24h");
    OPTIONS["24_HOUR"] = cOpt("interface", _("Time format"),
                              _("12h: AM/PM, eg: 7:31 AM - Military: 24h Military, eg: 0731 - 24h: Normal 24h, eg: 7:31"),
                              "12h,military,24h", "12h"
                             );

    mOptionsSort["interface"]++;

    OPTIONS["FORCE_CAPITAL_YN"] = cOpt("interface", _("Force Y/N in prompts"),
                                       _("If true, Y/N prompts are case-sensitive and y and n are not accepted."),
                                       true
                                      );

    OPTIONS["SNAP_TO_TARGET"] = cOpt("interface", _("Snap to target"),
                                     _("If true, automatically follow the crosshair when firing/throwing."),
                                     false
                                    );

    OPTIONS["SAVE_SLEEP"] = cOpt("interface", _("Ask to save before sleeping"),
                                 _("If true, game will ask to save the map before sleeping."),
                                 false
                                );

    OPTIONS["QUERY_DISASSEMBLE"] = cOpt("interface", _("Query on disassembly"),
                                        _("If true, will query before disassembling items."),
                                        true
                                       );

    OPTIONS["QUERY_KEYBIND_REMOVAL"] = cOpt("interface",
                                            _("Query on keybinding removal"),
                                            _("If true, will query before removing a keybinding from a hotkey."),
                                            true
                                           );

    OPTIONS["CLOSE_ADV_INV"] = cOpt("interface", _("Close advanced inventory on move all"),
                                    _("If true, will close the advanced inventory when the move all items command is used."),
                                    false
                                   );

    mOptionsSort["interface"]++;

    OPTIONS["VEHICLE_ARMOR_COLOR"] = cOpt("interface", _("Vehicle plating changes part color"),
                                          _("If true, vehicle parts will change color if they are armor plated"),
                                          true
                                         );

    OPTIONS["DRIVING_VIEW_OFFSET"] = cOpt("interface", _("Auto-shift the view while driving"),
                                          _("If true, view will automatically shift towards the driving direction"),
                                          true
                                         );

    OPTIONS["VEHICLE_DIR_INDICATOR"] = cOpt("interface", _("Draw vehicle facing indicator"),
                                            _("If true, when controlling a vehicle, a white 'X' at distance 10 from the center will display its current facing."),
                                            false
                                           );

    mOptionsSort["interface"]++;

    //~ sidebar style
    optionNames["wider"] = _("Wider");
    optionNames["narrow"] = _("Narrow");
    OPTIONS["SIDEBAR_STYLE"] = cOpt("interface", _("Sidebar style"),
                                    _("Switch between a narrower or wider sidebar. Requires restart."),
                                    "wider,narrow", "narrow"
                                   );

    //~ style of vehicle interaction menu; vertical is old one.
    optionNames["vertical"] = _("Vertical");
    optionNames["horizontal"] = _("Horizontal");
    optionNames["hybrid"] = _("Hybrid");
    OPTIONS["VEH_MENU_STYLE"] = cOpt("interface", _("Vehicle menu style"),
                                     _("Switch between two different styles of vehicle interaction menu or combination of them."),
                                     "vertical,horizontal,hybrid", "vertical"
                                    );

    mOptionsSort["interface"]++;

    OPTIONS["MOVE_VIEW_OFFSET"] = cOpt("interface", _("Move view offset"),
                                       _("Move view by how many squares per keypress."),
                                       1, 50, 1
                                      );

    OPTIONS["MENU_SCROLL"] = cOpt("interface", _("Centered menu scrolling"),
                                  _("If true, menus will start scrolling in the center of the list, and keep the list centered."),
                                  true
                                 );

    optionNames["false"] = _("False");
    optionNames["centered"] = _("Centered");
    optionNames["edge"] = _("To edge");
    OPTIONS["SHIFT_LIST_ITEM_VIEW"] = cOpt("interface", _("Shift list item view"),
                                           _("Centered or to edge, shift the view toward the selected item if it is outside of your current viewport."),
                                           "false,centered,edge",  "centered"
                                          );

    mOptionsSort["interface"]++;

    OPTIONS["ENABLE_JOYSTICK"] = cOpt("interface", _("Enable Joystick"),
                                      _("SDL ONLY: Enable input from joystick."),
                                      true, COPT_CURSES_HIDE
                                     );

    //~ show mouse cursor
    optionNames["show"] = _("Show");
    //~ hide mouse cursor
    optionNames["hide"] = _("Hide");
    //~ hide mouse cursor when keyboard is used
    optionNames["hidekb"] = _("HideKB");
    OPTIONS["HIDE_CURSOR"] = cOpt("interface", _("Hide mouse cursor"),
                                  _("Always: Cursor is always shown. Hidden: Cursor is hidden. HiddenKB: Cursor is hidden on keyboard input and unhidden on mouse movement."),
                                  "show,hide,hidekb", "show", COPT_CURSES_HIDE
                                 );

    ////////////////////////////GRAPHICS/////////////////////////
    OPTIONS["NO_BRIGHT_BACKGROUNDS"] = cOpt("graphics", _("No bright backgrounds"),
                                            _("If true, bright backgrounds are not used - some consoles are not compatible."),
                                            false
                                           );

    mOptionsSort["graphics"]++;

    OPTIONS["ANIMATIONS"] = cOpt("graphics", _("Animations"),
                                 _("If true, will display enabled animations."),
                                 true
                                );

    OPTIONS["ANIMATION_RAIN"] = cOpt("graphics", _("Rain animation"),
                                     _("If true, will display weather animations."),
                                     true
                                    );

    OPTIONS["ANIMATION_SCT"] = cOpt("graphics", _("SCT animation"),
                                    _("If true, will display scrolling combat text animations."),
                                    true
                                   );

    OPTIONS["ANIMATION_DELAY"] = cOpt("graphics", _("Animation delay"),
                                      _("The amount of time to pause between animation frames in ms."),
                                      0, 100, 10
                                      );

    mOptionsSort["graphics"]++;

    OPTIONS["TERMINAL_X"] = cOpt("graphics", _("Terminal width"),
                                 _("SDL ONLY: Set the size of the terminal along the X axis. Requires restart. POSIX systems will use terminal size at startup."),
                                 80, 242, 80, COPT_CURSES_HIDE
                                );

    OPTIONS["TERMINAL_Y"] = cOpt("graphics", _("Terminal height"),
                                 _("SDL ONLY: Set the size of the terminal along the Y axis. Requires restart. POSIX systems will use terminal size at startup."),
                                 24, 187, 24, COPT_CURSES_HIDE
                                );

    mOptionsSort["graphics"]++;

    OPTIONS["USE_TILES"] = cOpt("graphics", _("Use tiles"),
                                _("If true, replaces some TTF rendered text with tiles. Only applicable on SDL builds."),
                                true, COPT_CURSES_HIDE
                               );

    OPTIONS["TILES"] = cOpt("graphics", _("Choose tileset"),
                            _("Choose the tileset you want to use. Only applicable on SDL builds."),
                            tileset_names, "hoder", COPT_CURSES_HIDE
                           ); // populate the options dynamically

    mOptionsSort["graphics"]++;

    OPTIONS["FULLSCREEN"] = cOpt("graphics", _("Fullscreen"),
                                 _("SDL ONLY: Starts Cataclysm in fullscreen-mode. Requires Restart."),
                                 false, COPT_CURSES_HIDE
                                );

    OPTIONS["SOFTWARE_RENDERING"] = cOpt("graphics", _("Software rendering"),
                                         _("SDL ONLY: Use software renderer instead of graphics card acceleration."),
                                         false, COPT_CURSES_HIDE
                                        );

    mOptionsSort["graphics"]++;

    OPTIONS["MUSIC_VOLUME"] = cOpt("graphics", _("Music Volume"),
                                   _("SDL ONLY: Adjust the volume of the music being played in the background."),
                                   0, 200, 100, COPT_CURSES_HIDE
                                  );

    ////////////////////////////DEBUG////////////////////////////
    OPTIONS["DISTANCE_INITIAL_VISIBILITY"] = cOpt("debug", _("Distance initial visibility"),
                                                  _("Determines the scope, which is known in the beginning of the game."),
                                                  3, 20, 15
                                                 );

    mOptionsSort["debug"]++;

    OPTIONS["INITIAL_POINTS"] = cOpt("debug", _("Initial points"),
                                     _("Initial points available on character generation."),
                                     0, 1000, 6
                                    );

    OPTIONS["MAX_TRAIT_POINTS"] = cOpt("debug", _("Maximum trait points"),
                                       _("Maximum trait points available for character generation."),
                                       0, 1000, 12
                                      );

    mOptionsSort["debug"]++;

    //~ plain, default, normal
    optionNames["vanilla"] = _("Vanilla");
    //~ capped at a value
    optionNames["capped"] = _("Capped");
    //~ based on intelligence
    optionNames["int"] = _("Int");
    //~ based on intelligence and capped
    optionNames["intcap"] = _("IntCap");
    optionNames["off"] = _("Off");
    OPTIONS["SKILL_RUST"] = cOpt("debug", _("Skill rust"),
                                 _("Set the level of skill rust. Vanilla: Vanilla Cataclysm - Capped: Capped at skill levels 2 - Int: Intelligence dependent - IntCap: Intelligence dependent, capped - Off: None at all."),
                                 "vanilla,capped,int,intcap,off", "int"
                                );

    ////////////////////////////WORLD DEFAULT////////////////////
    optionNames["no"] = _("No");
    optionNames["yes"] = _("Yes");
    optionNames["query"] = _("Query");
    OPTIONS["DELETE_WORLD"] = cOpt("world_default", _("Delete world"),
                                   _("Delete the world when the last active character dies."),
                                   "no,yes,query", "no"
                                  );

    mOptionsSort["world_default"]++;

    OPTIONS["CITY_SIZE"] = cOpt("world_default", _("Size of cities"),
                                _("A number determining how large cities are. Warning, large numbers lead to very slow mapgen."),
                                1, 16, 4
                               );

    OPTIONS["SPAWN_DENSITY"] = cOpt("world_default", _("Spawn rate scaling factor"),
                                    _("A scaling factor that determines density of monster spawns."),
                                    0.0, 50.0, 1.0, 0.1
                                   );

    OPTIONS["ITEM_SPAWNRATE"] = cOpt("world_default", _("Item spawn scaling factor"),
                                     _("A scaling factor that determines density of item spawns."),
                                     0.01, 10.0, 1.0, 0.01
                                    );

    mOptionsSort["world_default"]++;

    std::string region_ids("default");
    optionNames["default"] = "default";
    OPTIONS["DEFAULT_REGION"] = cOpt("world_default", _("Default region type"),
                                     _("(WIP feature) Determines terrain, shops, plants, and more."),
                                     region_ids, "default"
                                    );

    mOptionsSort["world_default"]++;

    OPTIONS["INITIAL_TIME"] = cOpt("world_default", _("Initial time"),
                                   _("Initial starting time of day on character generation."),
                                   0, 23, 8
                                  );

    optionNames["spring"] = _("Spring");
    optionNames["summer"] = _("Summer");
    optionNames["autumn"] = _("Autumn");
    optionNames["winter"] = _("Winter");
    OPTIONS["INITIAL_SEASON"] = cOpt("world_default", _("Initial season"),
                                     _("Initial starting season of day on character generation."),
                                     "spring,summer,autumn,winter", "spring");

    OPTIONS["SEASON_LENGTH"] = cOpt("world_default", _("Season length"),
                                    _("Season length, in days."),
                                    14, 127, 14
                                   );

    mOptionsSort["world_default"]++;

    OPTIONS["STATIC_SPAWN"] = cOpt("world_default", _("Static spawn"),
                                   _("Spawn zombies at game start instead of during game. Must reset world directory after changing for it to take effect."),
                                   true
                                  );

    OPTIONS["WANDER_SPAWNS"] = cOpt("world_default", _("Wander spawns"),
                                    _("Emulation of zombie hordes. Zombie spawn points wander around cities and may go to noise"),
                                    false
                                   );

    OPTIONS["CLASSIC_ZOMBIES"] = cOpt("world_default", _("Classic zombies"),
                                      _("Only spawn classic zombies and natural wildlife. Requires a reset of save folder to take effect. This disables certain buildings."),
                                      false
                                     );

    OPTIONS["BLACK_ROAD"] = cOpt("world_default", _("Surrounded start"),
                                 _("If true, spawn zombies at shelters. Makes the starting game a lot harder."),
                                 false
                                );

    mOptionsSort["world_default"]++;

    OPTIONS["STATIC_NPC"] = cOpt("world_default", _("Static npcs"),
                                 _("If true, the game will spawn static NPC at the start of the game, requires world reset."),
                                 false
                                );

    OPTIONS["RANDOM_NPC"] = cOpt("world_default", _("Random npcs"),
                                 _("If true, the game will randomly spawn NPC during gameplay."),
                                 false
                                );

    mOptionsSort["world_default"]++;

    OPTIONS["RAD_MUTATION"] = cOpt("world_default", _("Mutations by radiation"),
                                   _("If true, radiation causes the player to mutate."),
                                   true
                                  );

    for (unsigned i = 0; i < vPages.size(); ++i) {
        mPageItems[i].resize(mOptionsSort[vPages[i].first]);
    }

    std::map<std::string, cOpt> OPTIONS_ORDERED(OPTIONS.begin(), OPTIONS.end());
    for( auto iter = OPTIONS_ORDERED.begin(); iter != OPTIONS_ORDERED.end(); ++iter ) {
        for (unsigned i = 0; i < vPages.size(); ++i) {
            if (vPages[i].first == (iter->second).getPage()) {
                mPageItems[i][(iter->second).getSortPos()] = iter->first;
                break;
            }
        }
    }
}

void show_options(bool ingame)
{
    auto OPTIONS_OLD = OPTIONS;
    auto WOPTIONS_OLD = ACTIVE_WORLD_OPTIONS;
    if ( world_generator->active_world == NULL ) {
        ingame = false;
    }

    const int iTooltipHeight = 4;
    const int iContentHeight = FULL_SCREEN_HEIGHT - 3 - iTooltipHeight;

    const int iOffsetX = (TERMX > FULL_SCREEN_WIDTH) ? (TERMX - FULL_SCREEN_WIDTH) / 2 : 0;
    const int iOffsetY = (TERMY > FULL_SCREEN_HEIGHT) ? (TERMY - FULL_SCREEN_HEIGHT) / 2 : 0;

    std::map<int, bool> mapLines;
    mapLines[4] = true;
    mapLines[60] = true;

    WINDOW *w_options_border = newwin(FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH, iOffsetY, iOffsetX);

    WINDOW *w_options_tooltip = newwin(iTooltipHeight, FULL_SCREEN_WIDTH - 2, 1 + iOffsetY,
                                       1 + iOffsetX);
    WINDOW *w_options_header = newwin(1, FULL_SCREEN_WIDTH - 2, 1 + iTooltipHeight + iOffsetY,
                                      1 + iOffsetX);
    WINDOW *w_options = newwin(iContentHeight, FULL_SCREEN_WIDTH - 2,
                               iTooltipHeight + 2 + iOffsetY, 1 + iOffsetX);

    draw_border(w_options_border);
    mvwputch(w_options_border, iTooltipHeight + 1,  0, BORDER_COLOR, LINE_XXXO); // |-
    mvwputch(w_options_border, iTooltipHeight + 1, 79, BORDER_COLOR, LINE_XOXX); // -|

    for (std::map<int, bool>::iterator iter = mapLines.begin(); iter != mapLines.end(); ++iter) {
        mvwputch(w_options_border, FULL_SCREEN_HEIGHT - 1, iter->first + 1, BORDER_COLOR, LINE_XXOX); // _|_
    }

    mvwprintz(w_options_border, 0, 36, c_ltred, _(" OPTIONS "));
    wrefresh(w_options_border);

    for (int i = 0; i < 78; i++) {
        if (mapLines[i]) {
            mvwputch(w_options_header, 0, i, BORDER_COLOR, LINE_OXXX);
        } else {
            mvwputch(w_options_header, 0, i, BORDER_COLOR, LINE_OXOX); // Draw header line
        }
    }

    wrefresh(w_options_header);

    int iCurrentPage = 0;
    int iLastPage = 0;
    int iCurrentLine = 0;
    int iStartPos = 0;
    bool bStuffChanged = false;
    bool bWorldStuffChanged = false;
    input_context ctxt("OPTIONS");
    ctxt.register_cardinal();
    ctxt.register_action("QUIT");
    ctxt.register_action("NEXT_TAB");
    ctxt.register_action("PREV_TAB");
    ctxt.register_action("CONFIRM");
    ctxt.register_action("HELP_KEYBINDINGS");

    std::stringstream sTemp;

    used_tiles_changed = false;

    while(true) {
        auto &cOPTIONS = ( ingame && iCurrentPage == iWorldOptPage ?
                           ACTIVE_WORLD_OPTIONS : OPTIONS );

        //Clear the lines
        for (int i = 0; i < iContentHeight; i++) {
            for (int j = 0; j < 79; j++) {
                if (mapLines[j]) {
                    mvwputch(w_options, i, j, BORDER_COLOR, LINE_XOXO);
                } else {
                    mvwputch(w_options, i, j, c_black, ' ');
                }

                if (i < iTooltipHeight) {
                    mvwputch(w_options_tooltip, i, j, c_black, ' ');
                }
            }
        }

        calcStartPos(iStartPos, iCurrentLine, iContentHeight, mPageItems[iCurrentPage].size());

        //Draw options
        bool was_blank_line = false; // Is prev. line was blank linei?.
        size_t hidden_counter = 0; // Counter of hidden options.
        size_t blanklines_counter = 0; // Counter of double blank lines.
        size_t iBlankOffset = 0; // Offset when blank line is printed.
        for (int i = iStartPos; i < iStartPos + ((iContentHeight > mPageItems[iCurrentPage].size()) ?
                mPageItems[iCurrentPage].size() : iContentHeight); i++) {

            int line_pos; // Current line position in window.
            nc_color cLineColor = c_ltgreen;
            cOpt *current_opt = &(cOPTIONS[mPageItems[iCurrentPage][i]]);

            if(current_opt->is_hidden()) {
                ++hidden_counter;
                DebugLog() << "Hidden increment. Current: " << hidden_counter;
                DebugLog() << "End iteration" << '\n';
                continue;
            }

            if (current_opt->getMenuText() == "") {
                if (was_blank_line) {
                    blanklines_counter++;
                }
                iBlankOffset++;
                was_blank_line = true;
                continue;
            }
            was_blank_line = false;

            line_pos = i - iStartPos - hidden_counter - blanklines_counter;

            sTemp.str("");
            sTemp << i + 1 - iBlankOffset - hidden_counter;
            mvwprintz(w_options, line_pos, 1, c_white, sTemp.str().c_str());
            mvwprintz(w_options, line_pos, 5, c_white, "");

            if (iCurrentLine == i) {
                wprintz(w_options, c_yellow, ">> ");
            } else {
                wprintz(w_options, c_yellow, "   ");
            }
            wprintz(w_options, c_white, "%s", current_opt->getMenuText().c_str());

            if (current_opt->getValue() == "false") {
                cLineColor = c_ltred;
            }

            mvwprintz(w_options, line_pos, 62, (iCurrentLine == i) ? hilite(cLineColor) :
                      cLineColor, "%s", current_opt->getValueName().c_str());
        }

        //Draw Scrollbar
        draw_scrollbar(w_options_border, iCurrentLine, iContentHeight,
                       mPageItems[iCurrentPage].size(), iTooltipHeight + 2, 0, BORDER_COLOR);

        //Draw Tabs
        mvwprintz(w_options_header, 0, 7, c_white, "");
        for (int i = 0; i < (int)vPages.size(); i++) {
            if (!mPageItems[i].empty()) { //skip empty pages
                wprintz(w_options_header, c_white, "[");
                if ( ingame && i == iWorldOptPage ) {
                    wprintz(w_options_header,
                            (iCurrentPage == i) ? hilite(c_ltgreen) : c_ltgreen, _("Current world"));
                } else {
                    wprintz(w_options_header, (iCurrentPage == i) ?
                            hilite(c_ltgreen) : c_ltgreen, (vPages[i].second).c_str());
                }
                wprintz(w_options_header, c_white, "]");
                wputch(w_options_header, BORDER_COLOR, LINE_OXOX);
            }
        }

        wrefresh(w_options_header);

#if (defined TILES || defined SDLTILES || defined _WIN32 || defined WINDOWS)
        if (mPageItems[iCurrentPage][iCurrentLine] == "TERMINAL_X") {
            int new_terminal_x, new_window_width;
            std::stringstream value_conversion(OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getValueName());

            value_conversion >> new_terminal_x;
            new_window_width = projected_window_width(new_terminal_x);

            fold_and_print(w_options_tooltip, 0, 0, 78, c_white,
                           ngettext("%s #%s -- The window will be %d pixel wide with the selected value.",
                                    "%s #%s -- The window will be %d pixels wide with the selected value.",
                                    new_window_width),
                           OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getTooltip().c_str(),
                           OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getDefaultText().c_str(),
                           new_window_width);
        } else if (mPageItems[iCurrentPage][iCurrentLine] == "TERMINAL_Y") {
            int new_terminal_y, new_window_height;
            std::stringstream value_conversion(OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getValueName());

            value_conversion >> new_terminal_y;
            new_window_height = projected_window_height(new_terminal_y);

            fold_and_print(w_options_tooltip, 0, 0, 78, c_white,
                           ngettext("%s #%s -- The window will be %d pixel tall with the selected value.",
                                    "%s #%s -- The window will be %d pixels tall with the selected value.",
                                    new_window_height),
                           OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getTooltip().c_str(),
                           OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getDefaultText().c_str(),
                           new_window_height);
        } else
#endif
        {
            fold_and_print(w_options_tooltip, 0, 0, 78, c_white, "%s #%s",
                           OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getTooltip().c_str(),
                           OPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getDefaultText().c_str());
        }

        if ( iCurrentPage != iLastPage ) {
            iLastPage = iCurrentPage;
            if ( ingame && iCurrentPage == iWorldOptPage ) {
                mvwprintz( w_options_tooltip, 3, 3, c_ltred, "%s", _("Note: ") );
                wprintz(  w_options_tooltip, c_white, "%s",
                          _("Some of these options may produce unexpected results if changed."));
            }
        }
        wrefresh(w_options_tooltip);

        wrefresh(w_options);

        const std::string action = ctxt.handle_input();

        bool bChangedSomething = false;
        if (action == "DOWN") {
            do {
                iCurrentLine++;
                if (iCurrentLine >= mPageItems[iCurrentPage].size()) {
                    iCurrentLine = 0;
                }
            } while(cOPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getMenuText() == "");
        } else if (action == "UP") {
            do {
                iCurrentLine--;
                if (iCurrentLine < 0) {
                    iCurrentLine = mPageItems[iCurrentPage].size() - 1;
                }
            } while(cOPTIONS[mPageItems[iCurrentPage][iCurrentLine]].getMenuText() == "");
        } else if (!mPageItems[iCurrentPage].empty() && action == "RIGHT") {
            cOPTIONS[mPageItems[iCurrentPage][iCurrentLine]].setNext();
            bChangedSomething = true;
        } else if (!mPageItems[iCurrentPage].empty() && action == "LEFT") {
            cOPTIONS[mPageItems[iCurrentPage][iCurrentLine]].setPrev();
            bChangedSomething = true;
        } else if (action == "NEXT_TAB") {
            iCurrentLine = 0;
            iStartPos = 0;
            iCurrentPage++;
            if (iCurrentPage >= vPages.size()) {
                iCurrentPage = 0;
            }
        } else if (action == "PREV_TAB") {
            iCurrentLine = 0;
            iStartPos = 0;
            iCurrentPage--;
            if (iCurrentPage < 0) {
                iCurrentPage = vPages.size() - 1;
            }
        } else if (!mPageItems[iCurrentPage].empty() && action == "CONFIRM") {
            cOpt &cur_opt = cOPTIONS[mPageItems[iCurrentPage][iCurrentLine]];
            if (cur_opt.getType() == "bool" || cur_opt.getType() == "string") {
                cur_opt.setNext();
                bChangedSomething = true;
            } else {
                const bool is_int = cur_opt.getType() == "int";
                const bool is_float = cur_opt.getType() == "float";
                const std::string old_opt_val = cur_opt.getValueName();
                const std::string opt_val = string_input_popup(
                                                cur_opt.getMenuText(), 80, old_opt_val, "", "", -1, is_int);
                if (!opt_val.empty() && opt_val != old_opt_val) {
                    if (is_float) {
                        std::istringstream ssTemp(opt_val);
                        ssTemp.imbue(std::locale(""));
                        // This uses the current locale, to allow the users
                        // to use their own decimal format.
                        float tmpFloat;
                        ssTemp >> tmpFloat;
                        if (ssTemp) {
                            cur_opt.setValue(tmpFloat);
                            bChangedSomething = true;
                        } else {
                            popup(_("Invalid input: not a number"));
                        }
                    } else {
                        // option is of type "int": string_input_popup
                        // has taken care that the string contains
                        // only digits, parsing is done in setValue
                        cur_opt.setValue(opt_val);
                        bChangedSomething = true;
                    }
                }
            }
        } else if (action == "QUIT") {
            break;
        }
        if(bChangedSomething) {
            bStuffChanged = true;
            if ( iCurrentPage == iWorldOptPage ) {
                bWorldStuffChanged = true;
            }
        }
    }

    used_tiles_changed = (OPTIONS_OLD["TILES"].getValue() != OPTIONS["TILES"].getValue()) ||
                         (OPTIONS_OLD["USE_TILES"] != OPTIONS["USE_TILES"]);
    bool lang_changed = OPTIONS_OLD["USE_LANG"].getValue() != OPTIONS["USE_LANG"].getValue();
    if (bStuffChanged) {
        if(query_yn(_("Save changes?"))) {
            save_options(ingame && bWorldStuffChanged);
        } else {
            used_tiles_changed = false;
            OPTIONS = OPTIONS_OLD;
            if (ingame && bWorldStuffChanged) {
                ACTIVE_WORLD_OPTIONS = WOPTIONS_OLD;
            }
        }
    }
    if( lang_changed ) {
        setlocale( LC_ALL, OPTIONS["USE_LANG"].getValue().c_str() );
    }
#ifdef SDLTILES
    if( used_tiles_changed ) {
        //try and keep SDL calls limited to source files that deal specifically with them
        try {
            tilecontext->reinit( "gfx" );
            g->init_ui();
            if( ingame ) {
                g->refresh_all();
            }
        } catch(std::string err) {
            popup(_("Loading the tileset failed: %s"), err.c_str());
            use_tiles = false;
        }
    }
#endif // SDLTILES
    delwin(w_options);
    delwin(w_options_border);
    delwin(w_options_header);
    delwin(w_options_tooltip);
}

void load_options()
{
    std::ifstream fin;
    bool legacy_options_loaded = false;
    fin.open(FILENAMES["options"].c_str());
    if(!fin.is_open()) {
        // Try at the legacy location.
        fin.open(FILENAMES["legacy_options"].c_str());
        if(!fin.is_open()) {
            // Create it since it doesn't seem to exist.
            assure_dir_exist(FILENAMES["config_dir"]);
            save_options();
            fin.open(FILENAMES["options"].c_str());
            if(!fin.is_open()) {
                DebugLog() << "Could neither read nor create" << FILENAMES["options"].c_str() << "\n";
                return;
            }
        } else {
            legacy_options_loaded = true;
        }
    }

    std::string sLine;
    while(!fin.eof()) {
        getline(fin, sLine);

        if(sLine != "" && sLine[0] != '#' && std::count(sLine.begin(), sLine.end(), ' ') == 1) {
            int iPos = sLine.find(' ');
            const std::string loadedvar = sLine.substr(0, iPos);
            const std::string loadedval = sLine.substr(iPos + 1, sLine.length());
            // option with values from post init() might get clobbered
            optionsdata.add_retry(loadedvar, loadedval); // stash it until update();

            OPTIONS[ loadedvar ].setValue( loadedval );
        }
    }

    fin.close();
    if( legacy_options_loaded ) {
        // Write out options file at new location.
        assure_dir_exist(FILENAMES["config_dir"]);
        save_options();
    }

    trigdist = OPTIONS["CIRCLEDIST"]; // cache to global due to heavy usage.
    use_tiles = OPTIONS["USE_TILES"]; // cache to global due to heavy usage.
}

std::string options_header()
{
    return "\
# This is the options file.  It works similarly to keymap.txt: the format is\n\
# <option name> <option value>\n\
# <option value> may be any number, positive or negative.  If you use a\n\
# negative sign, do not put a space between it and the number.\n\
#\n\
# If # is at the start of a line, it is considered a comment and is ignored.\n\
# In-line commenting is not allowed.  I think.\n\
#\n\
# If you want to restore the default options, simply delete this file.\n\
# A new options.txt will be created next time you play.\n\
\n\
";
}

void save_options(bool ingame)
{
    std::ofstream fout;
    fout.open(FILENAMES["options"].c_str());
    if(!fout.is_open()) {
        return;
    }

    fout << options_header() << std::endl;

    for( size_t j = 0; j < vPages.size(); ++j ) {
        bool update_wopt = (ingame && j == iWorldOptPage );
        for( size_t i = 0; i < mPageItems[j].size(); ++i ) {
            if (OPTIONS[mPageItems[j][i]].getDefaultText() != "") {
                fout << "#" << OPTIONS[mPageItems[j][i]].getTooltip() << std::endl;
                fout << "#" << OPTIONS[mPageItems[j][i]].getDefaultText(false) << std::endl;
                fout << mPageItems[j][i] << " " << OPTIONS[mPageItems[j][i]].getValue() << std::endl << std::endl;
                if ( update_wopt ) {
                    world_generator->active_world->world_options[ mPageItems[j][i] ] =
                        ACTIVE_WORLD_OPTIONS[ mPageItems[j][i] ];
                }
            }
        }
    }

    fout.close();
    if ( ingame ) {
        world_generator->save_world( world_generator->active_world, false );
    }
    trigdist = OPTIONS["CIRCLEDIST"]; // update trigdist as well
    use_tiles = OPTIONS["USE_TILES"]; // and use_tiles
}

bool use_narrow_sidebar()
{
    return TERMY < 25 || g->narrow_sidebar;
}

std::string get_tileset_names(std::string dir_path)
{
    const std::string defaultTilesets = "hoder,deon";

    const std::string filename = "tileset.txt";                             // tileset-info-file
    std::vector<std::string> files;
    files = file_finder::get_files_from_path(filename, dir_path, true);     // search it

    std::string tileset_names;
    bool first_tileset_name = true;

    for(std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it) {
        std::ifstream fin;
        fin.open(it->c_str());
        if(!fin.is_open()) {
            fin.close();
            DebugLog() << "\tCould not read ." << *it;
            optionNames["deon"] = _("Deon's");          // just setting some standards
            optionNames["hoder"] = _("Hoder's");
            return defaultTilesets;
        }
        // should only have 2 values inside it, otherwise is going to only load the last 2 values
        std::string tileset_name;

        while(!fin.eof()) {
            std::string sOption;
            fin >> sOption;

            //DebugLog() << "\tCurrent: " << sOption << " -- ";

            if(sOption == "") {
                getline(fin, sOption);    // Empty line, chomp it
                //DebugLog() << "Empty line, skipping\n";
            } else if(sOption[0] == '#') { // # indicates a comment
                getline(fin, sOption);
                //DebugLog() << "Comment line, skipping\n";
            } else {
                if (sOption.find("NAME") != std::string::npos) {
                    tileset_name = "";
                    fin >> tileset_name;
                    if(first_tileset_name) {
                        first_tileset_name = false;
                        tileset_names += tileset_name;
                    } else {
                        tileset_names += std::string(",");
                        tileset_names += tileset_name;
                    }
                } else if (sOption.find("VIEW") != std::string::npos) {
                    std::string viewName = "";
                    fin >> viewName;
                    optionNames[tileset_name] = viewName;
                    break;
                }
            }
        }
        fin.close();
    }

    if(tileset_names == "") {
        optionNames["deon"] = _("Deon's");          // more standards
        optionNames["hoder"] = _("Hoder's");
        return defaultTilesets;

    }

    return tileset_names;
}

void options_data::enable_json(const std::string &lvar)
{
    post_json_verify[ lvar ] = std::string( 1, 001 ); // because "" might be valid
}

void options_data::add_retry(const std::string &lvar, const::std::string &lval)
{
    static const std::string blank_value( 1, 001 );
    std::map<std::string, std::string>::const_iterator it = post_json_verify.find(lvar);
    if ( it != post_json_verify.end() && it->second == blank_value ) {
        // initialized with impossible value: valid
        post_json_verify[ lvar ] = lval;
    }
}

void options_data::add_value( const std::string &lvar, const std::string &lval,
                              std::string lvalname )
{
    static const std::string blank_value( 1, 001 );

    std::map<std::string, std::string>::const_iterator it = post_json_verify.find(lvar);
    if ( it != post_json_verify.end() ) {
        auto ot = OPTIONS.find(lvar);
        if ( ot != OPTIONS.end() && ot->second.sType == "string" ) {
            for(std::vector<std::string>::const_iterator eit = ot->second.vItems.begin();
                eit != ot->second.vItems.end(); ++eit) {
                if ( *eit == lval ) { // already in
                    return;
                }
            }
            ot->second.vItems.push_back(lval);
            if ( optionNames.find(lval) == optionNames.end() ) {
                optionNames[ lval ] = ( lvalname == "" ? lval : lvalname );
            }
            // our value was saved, then set to default, so set it again.
            if ( it->second == lval ) {
                OPTIONS[ lvar ].setValue( lval );
            }
        }

    }
}
