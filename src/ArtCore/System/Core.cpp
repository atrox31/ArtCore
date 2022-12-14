// ReSharper disable CppUseAuto
#include "Core.h"
#include "Core.h"

#include "ArtCore/Graphic/Render.h"
#include "ArtCore/System/AssetManager.h"
#include "ArtCore/_Debug/Debug.h"
#include "ArtCore/CodeExecutor/CodeExecutor.h"
#include "ArtCore/main.h" // for program version
#include "ArtCore/Graphic/BackGroundRenderer.h"
#include "ArtCore/Scene/Scene.h"
#include "ArtCore/Physics/Physics.h"

#include "ArtCore/predefined_headers/SplashScreen.h"
#include "ArtCore/Graphic/ColorDefinitions.h"

#include "physfs-release-3.2.0/src/physfs.h"

#ifdef _DEBUG
// time measurment of core events
#include "ArtCore/_Debug/Time.h"
#endif

Core Core::_instance = Core();

void Core::graphic::Apply()
{
    GPU_SetFullscreen(_window_fullscreen, false);
    GPU_SetWindowResolution(static_cast<Uint16>(_window_width), static_cast<Uint16>(_window_height));
    if (!_window_fullscreen)
        SDL_SetWindowPosition(Core::GetWindowHandle(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    if (_window_v_sync == 0) {
        SDL_GL_SetSwapInterval(0);
    }
    else {
        SDL_GL_SetSwapInterval(1);
    }
    Render::CreateRender(_window_width, _window_height);
    _screen_rect.X = 0.f;
    _screen_rect.Y = 0.f;
    _screen_rect.W = static_cast<float>(Core::SD_GetInt("DefaultResolutionX", 1920));
    _screen_rect.H = static_cast<float>(Core::SD_GetInt("DefaultResolutionY", 1080));
    //_screen_rect.W = static_cast<float>(_window_width);
    //_screen_rect.H = static_cast<float>(_window_height);
}

void Core::audio::Apply() const
{
    Mix_MasterVolume(
        ((_audio_master && _audio_sound) ? static_cast<int>(Func::LinearScale(
            static_cast<float>(_audio_sound_level),
            0.f,
            100.f,
            0.f,
            static_cast<float>(MIX_MAX_VOLUME)
        ))
            : 0));

    Mix_VolumeMusic(
        ((_audio_master && _audio_music) ? static_cast<int>(Func::LinearScale(
        static_cast<float>(_audio_music_level),
        0.f,
        100.f,
        0.f,
        static_cast<float>(MIX_MAX_VOLUME)
    ))
        : 0));
    
}

Core::Core()
{
    game_loop = false;
    _window = nullptr;
    _screenTarget = nullptr;
    DeltaTime = 0.0;
    LAST = 0;
    NOW = 0;
    _global_font = nullptr;
    fps = 0;
    _frames = 0;
    _current_scene = nullptr;
    _show_fps = false; 
    _asset_manager = nullptr;
    _executor = nullptr;
}

Core::~Core()
{
    if(_executor != nullptr)
		_executor->Delete();
    delete _executor;

    if(_asset_manager != nullptr)
		_asset_manager->ClearData();
    delete _asset_manager;

    if (_current_scene != nullptr)
    	_current_scene->Exit();
	delete _current_scene;

    Render::DestroyRender();

    if(_screenTarget != nullptr)
		GPU_FreeTarget(_screenTarget);
    //delete _screenTarget; heap error ?

    SettingsData.clear();
    Console::Exit();

    if (_global_font != nullptr)
        FC_FreeFont(_global_font);

    if (_window) {
        GPU_Quit();
    }
    IMG_Quit();
    TTF_Quit();
    Mix_CloseAudio();
    SDLNet_Quit();

    SDL_QuitSubSystem(SDL_INIT_TIMER);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_QuitSubSystem(SDL_INIT_HAPTIC);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);

    SDL_Quit();
}

// try to initialize critic game system
// break loading when error
#define TRY_TO_INIT_CRITIC(init, failure_msg, component)        \
 if (!(init)) {                                                 \
     Console::WriteLine(std::string( (component) )+": error."); \
     Console::WriteLine((failure_msg));                         \
     return false;                                              \
 } Console::WriteLine(std::string( (component) )+": success."); \

// try to initialize game system
// continue after error
#define TRY_TO_INIT(init, failure_msg, component)                \
 if (!(init)) {                                                  \
     Console::WriteLine(std::string( (component) )+": warning.");\
     Console::WriteLine((failure_msg));                          \
 } Console::WriteLine(std::string( (component) )+": success.");  \

// Converts arguments list to pairs, this allow to have nullptr if
// something is not populate.
void Core::PopulateArguments(const Func::str_vec& args)
{
    if (args.size() == 1) return;
    for (size_t i = 1; i < args.size();)
    {
        // is command?
        if(args[i][0] == '-')
        {
            // have next string?
	        if(i+1 < args.size())
	        {
                // next string is argument?
                if (args[i + 1][0] != '-')
                {
                    // insert command + argument
                    _program_arguments.emplace_back(args[i].c_str(), args[i + 1].c_str());
                    i += 2;
                    continue;
                }
                // next string is another command
                else
                {
                    // insert only command
                    _program_arguments.emplace_back(args[i].c_str(), nullptr);
                    i += 1;
                    continue;
                }
	        }
            // must be only command
            else
            {
                _program_arguments.emplace_back(args[i].c_str(), nullptr);
                i += 1;
                continue;
            }
        }
        // else not valid command
    }
}

Core::program_argument Core::GetProgramArgument(const std::string& argument)
{
	for (program_argument& program_argument : _program_arguments)
	{
		if(program_argument.first == argument)
		{
            return program_argument;
		}
	}
    return { nullptr, nullptr };
}

bool Core::LoadSetupFile(const char* platform, const std::string& setup_file)
{
    if(PHYSFS_mount(platform, nullptr, 0) == 0)
    {
        Console::WriteLine(std::string(PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())));
        return false;
    }
    if(PHYSFS_exists(setup_file.c_str()) == 0)
    {
        Console::WriteLine("File '"+setup_file+"' not found!");
        PHYSFS_unmount(platform);
	    return false;
    }
    for (const std::string& data : Func::ArchiveGetFileText(setup_file, nullptr, false)) {
        if (data.substr(0, 2) == "//") continue;

        Func::str_vec line = Func::Split(data, '=');
        if (line.empty()) continue;
        if (line.size() != 2) {
            Console::WriteLine("Cannot read property '" + data + "'");
            continue;
        }
        // open gl command
        if (line[0].substr(0, 2) == "SDL_GL_") {
            bool error = false;
            const SDL_GLattr attr = Func::GetSdlAttrFromString(line[0], &error);
            if (error) {
                Console::WriteLine("unknown property '" + data + "'");
            }
            else {
                if (SDL_GL_SetAttribute(attr, Func::TryGetInt(line[1])) != 0) {
                    Console::WriteLine({ "sdl_error: -SDL_GL_SetAttribute-", SDL_GetError() });
                }
            }
            continue;
        }
        // sdl command
        if (line[0].substr(0, 2) == "SDL_") {
            if (SDL_SetHint(line[0].c_str(), line[1].c_str()) == SDL_FALSE) {
                Console::WriteLine({ "sdl_error: -SDL_SetHint-", SDL_GetError() });
            }
            continue;
        }
        // other data
        {
            SD_SetValue(line[0], line[1]);
        }
    }

    PHYSFS_unmount(platform);
    return true;
}

bool Core::Init(const Func::str_vec& args)
{
#ifdef _DEBUG
    // debug timer to time all events in game
    Time timer;
    timer.StartTest();
#endif

	// default assets path and name
#ifdef _DEBUG
	const char* fl_game_dat_file = "test\\game.dat";
	const char* fl_assets_file = "test\\assets.pak";
	const char* fl_platform_file = "test\\Platform.dat";
#else //release
    const char* fl_game_dat_file = "game.dat";
    const char* fl_assets_file = "assets.pak";
    const char* fl_platform_file = "Platform.dat";
#endif

    // get command line arguments
    _instance.PopulateArguments(args);
    // first output then exit application
    if(const program_argument argument = _instance.GetProgramArgument("-version"); argument.first != nullptr)
    {
        SDL_Log("%d.%d.%d",VERSION_MAIN,VERSION_MINOR,VERSION_PATH);
        return false;
    }

    // initialize console to standard output
    Console::Create();

    if(const program_argument argument = _instance.GetProgramArgument("-platform"); argument.second != nullptr)
    {
        fl_platform_file = argument.second;
        Console::WriteLine("Fl_platform_file set to: '" + std::string(fl_platform_file) + "'");
    }
    if(const program_argument argument = _instance.GetProgramArgument("-game_dat"); argument.second != nullptr)
    {
        fl_game_dat_file = argument.second;
        Console::WriteLine("FL_game_dat_file set to: '" + std::string(fl_game_dat_file) + "'");
    }
    if(const program_argument argument = _instance.GetProgramArgument("-assets"); argument.second != nullptr)
    {
        fl_assets_file = argument.second;
        Console::WriteLine("FL_assets_file set to: '" + std::string(fl_assets_file) + "'");
    }
    if(const program_argument argument = _instance.GetProgramArgument("-debug"); argument.first != nullptr)
    {
        Console::SetOutputFile(
            argument.second == nullptr ? "console.log"
            : argument.second
        );
    }
    // sdl subsystem
    TRY_TO_INIT_CRITIC(
        SDL_InitSubSystem(SDL_INIT_VIDEO) == 0,
        std::string(SDL_GetError()),
        "SDL_Init SDL_INIT_VIDEO"
    )

        TRY_TO_INIT_CRITIC(
            PHYSFS_init(args[0].c_str()) != 0,
            std::string(PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())),
            "PHYSFS_init"
        )

        // platform configuration data
        TRY_TO_INIT(
            LoadSetupFile(fl_platform_file, "setup.ini"),
            "File not found, Using default settings",
            "Platform settings load"
        )

        // platform configuration data, can not exists
        TRY_TO_INIT(
            LoadSetupFile(fl_platform_file, "setup_user.ini"),
            "File not found, Using default settings",
            "Platform settings load"
        )
        

#ifdef _DEBUG
        GPU_SetDebugLevel(GPU_DEBUG_LEVEL_MAX);
#else
        GPU_SetDebugLevel(GPU_DEBUG_LEVEL_0);
#endif // _DEBUG

    _instance._screenTarget = GPU_Init(255, 255, GPU_INIT_DISABLE_VSYNC);
    TRY_TO_INIT_CRITIC(
        _instance._screenTarget != nullptr,
        std::string(SDL_GetError()),
        "GPU_Init"
    )
	_instance._window = SDL_GetWindowFromID(_instance._screenTarget->context->windowID);
    {
        // splash screen for core loading
		SDL_SetWindowBordered(_instance._window, SDL_FALSE);
        GPU_Clear(_instance._screenTarget);
        GPU_Image* splash = GPU_CopyImageFromSurface(SDL_LoadBMP_RW(SDL_RWFromConstMem(eula, 8190), 1));
        if (splash == nullptr)
        {
            Console::WriteLine("loading splash image:" + std::string(SDL_GetError()));
        }
        else
        {
            GPU_BlitRect(splash, nullptr, _instance._screenTarget, nullptr);
            GPU_Flip(_instance._screenTarget);
            GPU_Clear(_instance._screenTarget);
        }
        GPU_FreeImage(splash);
        splash = nullptr;
    }

    // SDL timer
    TRY_TO_INIT_CRITIC(
        SDL_InitSubSystem(SDL_INIT_TIMER) == 0,
        std::string(SDL_GetError()),
        "SDL_Init SDL_INIT_TIMER"
    )

    // SDL audio
    TRY_TO_INIT_CRITIC(
        SDL_InitSubSystem(SDL_INIT_AUDIO) == 0,
        std::string(SDL_GetError()),
        "SDL_Init SDL_INIT_AUDIO"
    )

    // SDL haptic - on mobile
	if (SD_GetInt("GameUsingHaptic", 0) == 1) {
		TRY_TO_INIT_CRITIC(
			SDL_InitSubSystem(SDL_INIT_HAPTIC) == 0,
			std::string(SDL_GetError()),
			"SDL_Init SDL_INIT_HAPTIC"
		)
	}

    // SDL controller
	if (SD_GetInt("GameUsingController", 0) == 1) {
		TRY_TO_INIT_CRITIC(
			SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0,
			std::string(SDL_GetError()),
			"SDL_Init SDL_INIT_GAMECONTROLLER"
		)
		TRY_TO_INIT_CRITIC(
            SDL_GameControllerAddMappingsFromRW( Func::ArchiveGetFileRWops("files/gamecontrollerdb.txt", nullptr), 1) > -1,
			std::string(SDL_GetError()),
			"SDL_GameControllerAddMappings"
		)
	}

    // IMG_Init
	TRY_TO_INIT_CRITIC(
        IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG,
        std::string(SDL_GetError()),
		"IMG_Init"
	)

    // Mix_Init
	TRY_TO_INIT_CRITIC(
        Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG) & (MIX_INIT_MP3 | MIX_INIT_OGG),
        std::string(SDL_GetError()),
		"Mix_Init"
	)

    // Mix_OpenAudio all sounds must be 32-bit integer samples and 44100 hz
	TRY_TO_INIT_CRITIC(
        Mix_OpenAudio(
            Core::SD_GetInt("AUDIO_FREQ",44100),
            AUDIO_S32LSB,
            2, 
            Core::SD_GetInt("AUDIO_CHUNKSIZE", 4096)
            ) == 0,
        std::string(SDL_GetError()),
		"Mix_OpenAudio"
    )

	// Mix_Init
	TRY_TO_INIT_CRITIC(
        TTF_Init() == 0,
		std::string(SDL_GetError()),
		"TTF_Init"
	)

	// SDLNet_Init
	TRY_TO_INIT_CRITIC(
        SDLNet_Init() == 0,
		std::string(SDL_GetError()),
		"SDLNet_Init"
	)

    // primary game data
    TRY_TO_INIT_CRITIC(
        PHYSFS_mount(fl_game_dat_file, nullptr, 0) != 0,
        std::string(PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())),
        "PHYSFS_mount '" + std::string(fl_game_dat_file) + "'"
    )

	// assets
	TRY_TO_INIT_CRITIC(
		PHYSFS_mount(fl_assets_file, nullptr, 0) != 0,
		std::string(PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode())),
		"PHYSFS_mount '" + std::string(fl_assets_file) + "'"
	)

    // default font
	_instance._global_font = FC_CreateFont();
	TRY_TO_INIT_CRITIC(
		FC_LoadFont_RW(_instance._global_font, Func::ArchiveGetFileRWops("files/TitilliumWeb-Light.ttf", nullptr), 1, 24, C_BLACK, TTF_STYLE_NORMAL) == 1,
		std::string(SDL_GetError()),
		"FC_LoadFont_RW: TitilliumWeb-Light.ttf"
	)
    FC_SetDefaultColor(_instance._global_font, C_BLACK);

    srand(static_cast<unsigned int>(time(nullptr)));
    Console::Init();

    _instance._asset_manager = new AssetManager();
    _instance._executor = new CodeExecutor();

	Graphic.SetScreenResolution(SD_GetInt("DefaultResolution_x", 1920), SD_GetInt("DefaultResolution_y", 1080));
    Graphic.SetFrameRate(SD_GetInt("DefaultFramerate", 60));
    Graphic.SetFullScreen(SD_GetInt("FullScreen", 0) == 1);

    Console::WriteLine("rdy");

#ifdef _DEBUG
    timer.EndTest();
    timer.PrintTest("Core::Init()");
#endif
    return true;
}

bool Core::ProcessCoreKeys(const Sint32 sym)
{
    if (sym == SDLK_INSERT) {
        _show_fps = !_show_fps;
        return true;
    }
    if (sym == SDLK_HOME) {
        Console::ConsoleHomeButtonPressed();
        return true;
    }
    return false;
}

bool Core::ProcessEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT)
        {
            Console::WriteLine("exit request");
            return true;
        }

        switch (e.type) {
        case SDL_KEYDOWN:
        {
#ifdef _DEBUG
            if (CoreDebug.ProcessEvent(&e)) {
                return false;
            }
#endif
            if (ProcessCoreKeys(e.key.keysym.sym)) break;
            if (Console::ProcessEvent(&e)) break;
        } break;

        case SDL_TEXTINPUT:
        {
            if (Console::ProcessEvent(&e)) break;
        } break;

		case SDL_MOUSEBUTTONDOWN:
        {
            if (e.button.button == SDL_BUTTON_LEFT) {
                Mouse.LeftEvent = Core::MouseState::ButtonState::PRESSED;
            }
            if (e.button.button == SDL_BUTTON_RIGHT) {
                Mouse.RightEvent = Core::MouseState::ButtonState::PRESSED;
            }
        } break;
        
        case SDL_MOUSEBUTTONUP: {
            if (e.button.button == SDL_BUTTON_LEFT) {
                Mouse.LeftEvent = Core::MouseState::ButtonState::RELEASED;
            }
            if (e.button.button == SDL_BUTTON_RIGHT) {
                Mouse.RightEvent = Core::MouseState::ButtonState::RELEASED;
            }
        } break;

        case SDL_KEYUP:
        case SDL_WINDOWEVENT:
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_RENDER_TARGETS_RESET:/**< The render targets have been reset and their contents need to be updated */
        case SDL_RENDER_DEVICE_RESET: /**< The device has been reset and all textures need to be recreated */
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_MOUSEWHEEL:
            break;
        default:continue;
        }
    }
    return false;
}

void Core::ProcessStep() const
{
    // interface (gui) events
    const bool gui_have_event = _current_scene->GuiSystem.Events();
    // add all new instances to scene and execute OnCreate event
    _current_scene->SpawnAll();
    if (_current_scene->IsAnyInstances()) {
        for (plf::colony<Instance*>::iterator it = _current_scene->InstanceColony.begin(); 
            it != _current_scene->InstanceColony.end();)
        {
            if (Instance* c_instance = (*it); c_instance->Alive) {
                // step
                Executor()->ExecuteScript(c_instance, Event::EvStep);
                const event_bit c_flag = c_instance->EventFlag;

                // in view
                if (c_instance->Alive) {
                    SDL_FPoint pos{ c_instance->PosX, c_instance->PosY };
                    const bool oldInView = c_instance->InView;
                    c_instance->InView = Graphic.GetScreenSpace()->PointInRect(pos);
                    if (c_instance->InView != oldInView) {
                        if (EVENT_BIT_TEST(event_bit::HAVE_VIEW_CHANGE, c_flag)) {
                            if (oldInView == true) { // be inside, now exit view
                                Executor()->ExecuteScript(c_instance, Event::EvOnViewLeave);
                            }
                            else {
                                Executor()->ExecuteScript(c_instance, Event::EvOnViewEnter);
                            }
                        }
                    }
                }

                // mouse input
                if (c_instance->Alive && (!gui_have_event)) {
	                if (EVENT_BIT_TEST(event_bit::HAVE_MOUSE_EVENT, c_flag)) {
		                if (Mouse.LeftEvent == MouseState::ButtonState::PRESSED) {
			                // global click
			                if (EVENT_BIT_TEST(event_bit::HAVE_MOUSE_EVENT_DOWN, c_flag)) {
				                Executor()->ExecuteScript(c_instance, Event::EvOnMouseDown);
			                }
			                // on mask click
			                if (EVENT_BIT_TEST(event_bit::HAVE_MOUSE_EVENT_CLICK, c_flag)) {
				                if (c_instance->CheckMaskClick(Mouse.XYf)) {
					                Executor()->ExecuteScript(c_instance, Event::EvClicked);
				                }
			                }
		                }
		                if (Mouse.LeftEvent == MouseState::ButtonState::RELEASED) {
			                if (EVENT_BIT_TEST(event_bit::HAVE_MOUSE_EVENT_UP, c_flag)) {
				                Executor()->ExecuteScript(c_instance, Event::EvOnMouseUp);
			                }
		                }
	                }
                }
                // go to next instance
                ++it;
            }
            else {
                // delete instance if not alive
                if (EVENT_BIT_TEST(event_bit::HAVE_ON_DESTROY, c_instance->EventFlag)) {
                    Executor()->ExecuteScript(c_instance, Event::EvOnDestroy);
                }
                delete (*it);
                it = _instance._current_scene->DeleteInstance(it);
            }
        }
        // execute all suspended code
		CodeExecutor::SuspendedCodeExecute();
    }
}
#include "ArtCore/_Debug/Time.h"
void Core::ProcessPhysics() const
{
    for (const auto instance : _current_scene->InstanceColony) {
        if (instance->Alive) {
            // collision
            if (EVENT_BIT_TEST(event_bit::HAVE_COLLISION, instance->EventFlag)) {
                for (Instance* target : _current_scene->InstanceColony) {
                    if (Physics::CollisionTest(instance, target)) {
                        _current_scene->CurrentCollisionInstance = target;
                        _current_scene->CurrentCollisionInstanceId = target->GetId();
                        Executor()->ExecuteScript(instance, Event::EvOnCollision);
                        _current_scene->CurrentCollisionInstance = nullptr;
                        _current_scene->CurrentCollisionInstanceId = -1;
                    }
                }
            }
        }
    }
}

void Core::ProcessSceneRender() const
{
    // render scene background
    if (_current_scene->BackGround.Texture != nullptr) {
        Render::DrawTextureBox(_current_scene->BackGround.Texture, nullptr, nullptr);
    }
    else
    {
        Render::RenderClearColor(_current_scene->BackGround.Color);
    }

    // draw all instances if in view (defined in step event)
    if (_current_scene->IsAnyInstances()) {
        for (Instance* instance : _current_scene->InstanceColony) {
            if (instance->InView) {
                Executor()->ExecuteScript(instance, Event::EvDraw);
            }
        }
    }
}

void Core::ProcessPostProcessRender() const
{
	// post process
    Render::ProcessImageWithGaussian();

	// draw interface
    _current_scene->GuiSystem.Render();

    // draw all scene to screen buffer
    Render::RenderToTarget(_screenTarget);
}

void Core::ProcessSystemRender() const
{
    // DEBUG DRAW
#ifdef _DEBUG
    _instance.CoreDebug.Draw();
#endif // _DEBUG

    // draw console
    Console::RenderConsole(_screenTarget);


    if (_instance._show_fps) {
        GPU_DeactivateShaderProgram();
        const std::string text = "FPS: " + std::to_string(fps);
        GPU_Rect info_rect = FC_GetBounds(_global_font, 0, 0, FC_ALIGN_LEFT, FC_Scale{ 1.2f, 1.2f }, text.c_str());
        constexpr float info_rect_move = 8.f;
        info_rect.x += info_rect_move;
        info_rect.y += info_rect_move;
        info_rect.w += info_rect_move;
        info_rect.h += info_rect_move;

        GPU_RectangleFilled2(_screenTarget, info_rect, C_BLACK);
        GPU_Rectangle2(_screenTarget, info_rect, C_DGREEN);

        FC_DrawEffect(_global_font, _screenTarget, info_rect.x + 4.f, info_rect.y, FC_MakeEffect(FC_ALIGN_LEFT, { 1.f,1.f }, C_DGREEN), text.c_str());
    }
}

Scene* Core::GetCurrentScene()
{
	return _instance._current_scene;
}


SDL_Window* Core::GetWindowHandle()
{
    if (_instance._screenTarget) {
        return SDL_GetWindowFromID(_instance._screenTarget->context->windowID);
    }
    return nullptr;
}

void Core::MouseState::Reset()
{
    int x{}, y{};
    const Uint32 button_state = SDL_GetMouseState(&x, &y);
    // TODO test this
    Mouse.XYf = Render::ScalePoint({ static_cast<float>(x) ,static_cast<float>(y) });
    Mouse.XY = { static_cast<int>(Mouse.XYf.x) ,static_cast<int>(Mouse.XYf.y) };

    Mouse.LeftPressed = (button_state == SDL_BUTTON(SDL_BUTTON_LEFT));
    Mouse.RightPressed = (button_state == SDL_BUTTON(SDL_BUTTON_RIGHT));
    Mouse.Wheel = 0;
    Mouse.LeftEvent = ButtonState::NONE;
    Mouse.RightEvent = ButtonState::NONE;
}

int Core::SD_GetInt(const std::string& field, const int _default)
{
    if (_instance.SettingsData.contains(field)) {
        return Func::TryGetInt(_instance.SettingsData[field]);
    }
    return _default;
}

float Core::SD_GetFloat(const std::string& field, const float _default)
{
    if (_instance.SettingsData.contains(field)) {
        return Func::TryGetFloat(_instance.SettingsData[field]);
    }
    return _default;
}

std::string Core::SD_GetString(const std::string& field, std::string _default)
{
    if (_instance.SettingsData.contains(field)) {
        return _instance.SettingsData[field];
    }
    return _default;
}

void Core::SD_SetValue(const std::string& field, const std::string& value)
{
    if (_instance.SettingsData.contains(field)) {
        if (_instance.SettingsData[field] != value) {
           //PHYSFS_Archiver
            _instance.SettingsData[field] = value;
        }
    }
    else {
        _instance.SettingsData.emplace(field, value);
    }
}

Uint32 Core::FpsCounterCallback(Uint32 interval, void*)
{
    Core::GetInstance()->fps = Core::GetInstance()->_frames;
    Core::GetInstance()->_frames = 0;
#ifdef _DEBUG
    Core::GetInstance()->CoreDebug.PerformanceTimeSecondPassed();
#endif
    return interval;
}

bool Core::Run()
{
    _instance.game_loop = true;
    SDL_TimerID my_timer_id = SDL_AddTimer(static_cast<Uint32>(1000), FpsCounterCallback, nullptr);
#ifdef _DEBUG
    Time performance_all;
    Time performance_step;
    Time performance_physics;
    Time performance_render;
    Time performance_post_process;
    Time performance_counter_gpu_flip;

#define debug_test_counter_start(counter)   \
	if(_instance.CoreDebug._show_performance_times) {   \
    (counter).StartTest();  \
};                      \

#define debug_test_counter_end(counter)     \
	if(_instance.CoreDebug._show_performance_times) {   \
    (counter).EndTest();    \
};     \

#define debug_test_counter_get(counter, target)             \
	if(_instance.CoreDebug._show_performance_times) {   \
    (target) += (counter).GetTestTime();    \
};     \

#else

#define debug_test_counter_start(counter)
#define debug_test_counter_end(counter) 
#define debug_test_counter_get(counter, target) 

#endif


    
    while (true) {
        debug_test_counter_start(performance_all);
        if (_instance._current_scene == nullptr) return EXIT_FAILURE;
        // FPS measurement
        _instance.LAST = _instance.NOW;
        _instance.NOW = SDL_GetTicks64();
        DeltaTime = (static_cast<double>(_instance.NOW - _instance.LAST) / 1000.0);
        _instance._frames++;

        // Set global mouse state
        Mouse.Reset();

        debug_test_counter_start(performance_step)
        if(_instance.ProcessEvents())
        { // exit call
            return true;
        }

        if (_instance.game_loop) {
            _instance.ProcessStep();
        debug_test_counter_end(performance_step)

        debug_test_counter_start(performance_physics)
            _instance.ProcessPhysics();
        debug_test_counter_end(performance_physics)
        }

        debug_test_counter_start(performance_render)
        Render::RenderClear();
        // render scene
        _instance.ProcessSceneRender();
        debug_test_counter_end(performance_render)


    	debug_test_counter_start(performance_post_process)
        // render interface and make scene pretty
        _instance.ProcessPostProcessRender();
        debug_test_counter_end(performance_post_process)

    	debug_test_counter_start(performance_counter_gpu_flip)
        // render console, debug panels etc
        _instance.ProcessSystemRender();

        // get all to screen
        GPU_Flip(_instance._screenTarget);
        debug_test_counter_end(performance_counter_gpu_flip)

        debug_test_counter_end(performance_all);


        debug_test_counter_get(performance_all, _instance.CoreDebug._performance_counter_all_rt);
        debug_test_counter_get(performance_step, _instance.CoreDebug._performance_counter_step_rt);
        debug_test_counter_get(performance_physics, _instance.CoreDebug._performance_counter_psychics_rt);
        debug_test_counter_get(performance_render, _instance.CoreDebug._performance_counter_render_rt);
        debug_test_counter_get(performance_post_process, _instance.CoreDebug._performance_counter_post_process_rt);
        debug_test_counter_get(performance_counter_gpu_flip, _instance.CoreDebug._performance_counter_gpu_flip_rt);
    }
}

bool Core::LoadData()
{
#ifdef _DEBUG
	Time timer;
	timer.StartTest();
#endif
	SDL_SetWindowBordered(_instance._window, SDL_TRUE);
	Graphic.Apply();
    Audio.Apply();
	GPU_Clear(GetScreenTarget());
	GPU_Flip(GetScreenTarget());

	BackGroundRenderer bgr = BackGroundRenderer();
	bgr.Run();
    
	if (!Executor()->LoadArtLib()) {
		bgr.Stop();
		return false;
	}
	bgr.SetProgress(5);
    
	// load assets
	if (!_instance._asset_manager->LoadData(&bgr, 5, 60)) {
		bgr.Stop();
		return false;
	}
	bgr.SetProgress(60);

	if (!Executor()->LoadObjectDefinitions(&bgr, 60, 90)) {
		bgr.Stop();
		return false;
	}
	bgr.SetProgress(90);

	// set starting scene
    {
        const Func::str_vec starting_scene = Func::ArchiveGetFileText("scene/StartingScene.txt", nullptr, false);
        if (starting_scene.empty()) {
            bgr.Stop();
            return false;
        }
        _instance._primary_scene = starting_scene[0];
    }

    // if error try set first scene
    if (!_instance.ChangeScene(_instance._primary_scene)) {
        Console::WriteLine("starting scene '" + _instance._primary_scene + "' not exists!");
        bgr.Stop();
        return false;
    }
    Render::LoadShaders();
    bgr.SetProgress(100);

    bgr.Stop();
#ifdef _DEBUG
    timer.EndTest();
    timer.PrintTest("Core::LoadData()");
#endif
    return true;
}

void Core::Exit()
{
    SDL_Event* sdl_event = new SDL_Event();
    sdl_event->type = SDL_QUIT;
    SDL_PushEvent(sdl_event);
}

bool Core::ChangeScene(const std::string& name)
{
    if (_instance._current_scene != nullptr) {
        // exit scene
        _current_scene->Exit();
    }
    CodeExecutor::SuspendedCodeStop();

    Scene* new_scene = new Scene();
    if (new_scene->Load(name))
    {
        _current_scene = new_scene;
        if (_current_scene->Start()) {
            return true;
        }
        delete new_scene;
        Console::WriteLine("[Core::ChangeScene] Error while starting new scene.");
    }
    else
    {
        Console::WriteLine("[Core::ChangeScene] scene '" + name + "' not exists! Try to load primary scene");
    }

    Scene* primary_scene = new Scene();
    if (primary_scene->Load(_primary_scene))
    {
        _current_scene = primary_scene;
        if (_current_scene->Start()) {
            return true;
        }
        Console::WriteLine("[Core::ChangeScene] Error while starting primary scene.");
    }
    else
    {
        Console::WriteLine("[Core::ChangeScene] Error while load primary scene.");
    }

    delete primary_scene;
    return false;
}
#ifdef _DEBUG
void Core::CoreDebug::PerformanceTimeSecondPassed()
{
    if (!_show_performance_times) return;
    _performance_counter_all =          _performance_counter_all_rt / _instance.fps;
	_performance_counter_step =         _performance_counter_step_rt / _instance.fps;
    _performance_counter_psychics =     _performance_counter_psychics_rt / _instance.fps;
    _performance_counter_render =       _performance_counter_render_rt / _instance.fps;
    _performance_counter_post_process = _performance_counter_post_process_rt / _instance.fps;
    _performance_counter_gpu_flip =     _performance_counter_gpu_flip_rt / _instance.fps;

    _performance_counter_other = 
        _performance_counter_all - _performance_counter_step - _performance_counter_psychics - 
        _performance_counter_render - _performance_counter_post_process - _performance_counter_gpu_flip;

    _performance_counter_all_rt = 0.0;
    _performance_counter_step_rt = 0.0;
    _performance_counter_psychics_rt = 0.0;
    _performance_counter_render_rt = 0.0;
    _performance_counter_post_process_rt = 0.0;
    _performance_counter_gpu_flip_rt = 0.0;
}
Core::CoreDebug::CoreDebug()
{
    Options.emplace_back( "Performance", &_show_performance, SDLK_F2);
    Options.emplace_back( "Draw Instance info", &_show_instance_info, SDLK_F3);
    Options.emplace_back( "Draw Collider`s", &_show_collider, SDLK_F4);
    Options.emplace_back( "Draw Direction`s", &_show_directions, SDLK_F5);
    Options.emplace_back( "Show spy window", &_show_spy_window, SDLK_F6);
    Options.emplace_back( "Show performance counters", &_show_performance_times, SDLK_F7);
}

bool Core::CoreDebug::ProcessEvent(SDL_Event* e)
{
    switch (e->type)
    {
    case SDL_MOUSEWHEEL:
	    {
		    
	    }
        break;
    case SDL_MOUSEBUTTONDOWN:
	    {

	    }
	    break;
    case SDL_KEYDOWN:
    {
        if (e->key.keysym.sym == SDLK_F1) {
            _show_debug_options = !_show_debug_options;
            return true;
        }
        for (const option& option : Options)
        {
            if (e->key.keysym.sym == option.Key)
            {
                *option.Anchor = !*option.Anchor;
                return true;
            }
        }
        if (e->key.keysym.sym == SDLK_PAGEUP)
        {
            if (_spy_line_begin > 0) {
                _spy_line_begin -= 1;
                _spy_line_end -= 1;
            }
            return true;
        }
        if (e->key.keysym.sym == SDLK_PAGEDOWN)
        {
            if (_spy_line_end < _spy_line_max) {
                _spy_line_begin += 1;
                _spy_line_end += 1;
            }
            return true;
        }

    }
    break;
    }
    return false;
}

void Core::CoreDebug::Draw() const
{
    if (_show_instance_info || _show_collider || _show_directions)
    {
        if (_instance._current_scene->IsAnyInstances()) {
            for (const Instance* instance : _instance._current_scene->InstanceColony) {
                GPU_Circle(_instance._screenTarget, instance->PosX, instance->PosY, 4, C_BLACK);
                // always draw origins point
            	if (instance->SelfSprite) {
                    GPU_Circle(_instance._screenTarget, instance->PosX + instance->SelfSprite->GetCenterXRel(), instance->PosY + instance->SelfSprite->GetCenterYRel(), 6, C_GOLD);
                }
                else {
                    GPU_Circle(_instance._screenTarget, instance->PosX, instance->PosY, 6, C_GOLD);
                }

                if(_show_directions)
                {
                    const float prv = GPU_SetLineThickness(4.f);
                    vec2f line_begin{ instance->PosX, instance->PosY };
                    vec2f line_end = Func::GetDirectionVector(instance->Direction);
                    line_end *= 42.f;
                    line_end += line_begin;
                    GPU_Line(_instance._screenTarget, line_begin.x, line_begin.y, line_end.x, line_end.y, C_DYELLOW);
                    GPU_SetLineThickness(prv);
                }

                if (_show_collider) {
                    if (instance->Body.Type == Instance::BodyType::Circle) {
                        const float radius_scale = instance->Body.Value * ((instance->SpriteScaleX + instance->SpriteScaleY) / 2.f);
                        GPU_Circle(_instance._screenTarget, instance->PosX, instance->PosY, radius_scale, C_BLUE);
                    }
                    if (instance->Body.Type == Instance::BodyType::Rect) {
                        GPU_Rectangle2(_instance._screenTarget, instance->GetBodyMask().ToGPU_Rect_wh(), C_BLUE);
                    }
                }

                if (_show_instance_info) {
                    const std::string text = instance->Name + "#" + std::to_string(instance->GetId()) + "[" + std::to_string(static_cast<int>(instance->PosX)) + "," + std::to_string(static_cast<int>(instance->PosY)) + "]";
                    GPU_Rect draw_surface = FC_GetBounds(_instance._global_font, 0.f, 0.f, FC_ALIGN_LEFT, { 1.f, 1.f }, text.c_str());
                    draw_surface.x = std::clamp(instance->PosX, 0.f, static_cast<float>(GetScreenWidth()) - draw_surface.w);
                    draw_surface.y = std::clamp(instance->PosY, 0.f, static_cast<float>(GetScreenHeight()) - draw_surface.h);
                    FC_DrawColor(_instance._global_font, _instance._screenTarget, draw_surface.x, draw_surface.y, C_RED, text.c_str());
                }
            }
        }
    }

    float draw_height = 0.f;

    if(_show_debug_options)
    {
        std::string output_text;
        for (auto& option : Options)
        {
            output_text += "[" + std::string((*option.Anchor) ? "X" : "  ") + "] " + option.Text + " | " + SDL_GetKeyName(option.Key)+ '\n';
        }
        GPU_Rect draw_surface = FC_GetBounds(_instance._global_font, 4.f, 14.f, FC_ALIGN_LEFT, { 1.f, 1.f }, output_text.c_str());

        draw_surface.y = 30.f;
        draw_surface.h += 30.f;

        const float moving = static_cast<float>(GetScreenWidth() - 20);
        draw_surface.x = moving - draw_surface.w;
        draw_surface.w += 18.f;

        GPU_RectangleFilled2(_instance._screenTarget, draw_surface, C_BLACK);
        GPU_Rectangle2(_instance._screenTarget, draw_surface, C_DGREEN);
        FC_DrawColor(_instance._global_font, _instance._screenTarget, draw_surface.x + 4.f, draw_surface.y + 10.f, C_DGREEN, output_text.c_str());

        GPU_Rect dev_text_frame = FC_GetBounds(_instance._global_font, 4.f, 4.f, FC_ALIGN_LEFT, { 1.2f, 1.2f }, "%s", "DEBUG MENU");
        dev_text_frame.x = draw_surface.x + 12.f;
        dev_text_frame.y = draw_surface.y - 28.f;
        GPU_RectangleFilled2(_instance._screenTarget, dev_text_frame, C_BLACK);
        GPU_Rectangle2(_instance._screenTarget, dev_text_frame, C_DGREEN);
        FC_DrawColor(_instance._global_font, _instance._screenTarget, dev_text_frame.x + 4.f, dev_text_frame.y + 4.f, C_DGREEN, "%s", "DEBUG MENU");
        draw_height += draw_surface.h + 8.f;
    }

    if(_show_performance)
    {
        const std::string text =
            "instance count[colony size]: " + std::to_string(_instance._current_scene->GetInstancesCount()) + '[' + std::to_string(_instance._current_scene->InstanceColony.size()) + ']' + '\n' +
            "delta time: " + std::to_string(_instance.DeltaTime) + '\n' +
            "Executor global stack size[capacity]: " + std::to_string(CodeExecutor::GetGlobalStackSize()) + '[' + std::to_string(CodeExecutor::GetGlobalStackSize()) + ']' + '\n' +
            "Executor if-test stack size: " + std::to_string(Core::Executor()->DebugGetIfTestResultStackSize()) + ']' + '\n' +
            "bloom draw: " + (_instance.use_bloom ? "enabled (" + std::to_string(_instance.use_bloom_level) + ")" : "disabled");

    	GPU_Rect info_rect = FC_GetBounds(_instance._global_font, 0.f, 0.f, FC_ALIGN_LEFT, FC_Scale{ 1.f, 1.f }, text.c_str());

        const float moving = static_cast<float>(GetScreenWidth() - 20);
        info_rect.x = moving - info_rect.w;
        info_rect.w += 18.f;

        info_rect.y += draw_height + 28.f;
        info_rect.h += 8.f;

        GPU_RectangleFilled2(_instance._screenTarget, info_rect, C_BLACK);
        GPU_Rectangle2(_instance._screenTarget, info_rect, C_DGREEN);
        FC_DrawColor(_instance._global_font, _instance._screenTarget, info_rect.x + 4, info_rect.y + 4, C_DGREEN, "%s", text.c_str());
        draw_height += info_rect.h + 8.f;
    }

    if(_show_spy_window)
    {
        const Func::str_vec text = Func::Split(Executor()->DebugGetTrackInfo(), '\n');
        std::string sliced_text;

        for(int i= _spy_line_begin; i< _spy_line_end; i++)
        {
            sliced_text += text[i] + '\n';
        }
       
        GPU_Rect info_rect = FC_GetBounds(_instance._global_font, 0.f, 0.f, FC_ALIGN_LEFT, FC_Scale{ 1.f, 1.f }, sliced_text.c_str());

    	const float moving = static_cast<float>(GetScreenWidth() - 20);
        info_rect.x = moving - info_rect.w;
        info_rect.w += 18.f;

        info_rect.y += draw_height + 28.f;
        info_rect.h += 8.f;

		GPU_RectangleFilled2(_instance._screenTarget, info_rect, C_BLACK);
        GPU_Rectangle2(_instance._screenTarget, info_rect, C_DGREEN);
        FC_DrawColor(_instance._global_font, _instance._screenTarget,  info_rect.x + 4, info_rect.y + 4 , C_DGREEN, "%s", sliced_text.c_str());

        FC_DrawEffect(_instance._global_font, _instance._screenTarget, info_rect.x + info_rect.w / 2.f, info_rect.h + info_rect.y - 42.f, { FC_ALIGN_CENTER, {1.f,1.f},C_DGREEN }, "<PG_UP> <PG_DOWN>");
    }
    if(_show_performance_times)
    {
        const std::string header = "Performance counters\n";
        const std::string text_left = 
            std::string("All time:")    + '\n' +
            std::string("Step events:") + '\n' +
            std::string("Physics:")     + '\n' +
            std::string("Render:")      + '\n' +
            std::string("Post process:")+ '\n' +
            std::string("GPU_Flip:")    + '\n' +
            std::string("Other:");

        const std::string text_right = 
            std::string("%.4f ms") + '\n' +
            std::string("%.4f ms") + '\n' +
            std::string("%.4f ms") + '\n' +
            std::string("%.4f ms") + '\n' +
            std::string("%.4f ms") + '\n' +
            std::string("%.4f ms") + '\n' +
            std::string("%.4f ms");

        GPU_Rect info_rect_left = FC_GetBounds(_instance._global_font, 0.f, 0.f, FC_ALIGN_LEFT, FC_Scale{ 1.f, 1.f }, text_left.c_str());
        GPU_Rect info_rect_right = FC_GetBounds(_instance._global_font, 0.f, 0.f, FC_ALIGN_RIGHT, FC_Scale{ 1.f, 1.f }, text_right.c_str(),
            _performance_counter_all,
            _performance_counter_step,
            _performance_counter_psychics,
            _performance_counter_render,
            _performance_counter_post_process,
            _performance_counter_gpu_flip,
            _performance_counter_other
            );
        
        GPU_Rect info_rect = {
            16.f, 16.f + (_instance._show_fps ? 64.f : 0.f),
            info_rect_left.w + 16.f + info_rect_right.w,
            std::max(info_rect_left.h, info_rect_right.h) + 16.f
        };

        const double percent_step = _performance_counter_step / _performance_counter_all * 360.0;
        const double percent_psychics = _performance_counter_psychics / _performance_counter_all * 360.0;
        const double percent_render = _performance_counter_render / _performance_counter_all * 360.0;
        const double percent_post_process = _performance_counter_post_process / _performance_counter_all * 360.0;
        const double percent_gpu_flip = _performance_counter_gpu_flip / _performance_counter_all * 360.0;
        const double percent_other = _performance_counter_other / _performance_counter_all * 360.0;

        float angle = 0.f;

        const SDL_Color color_table[] = {
            C_LGREEN, C_GREEN, C_RED, C_GOLD, C_BLUE, C_DYELLOW, C_GRAY
        };

        GPU_ArcFilled(_instance._screenTarget, info_rect.x + 128.f, info_rect.y + info_rect.h + 128.f + 16.f,
            128.f, angle, angle + static_cast<float>(percent_step),
            color_table[1]);
        angle += static_cast<float>(percent_step);

        GPU_ArcFilled(_instance._screenTarget, info_rect.x + 128.f, info_rect.y + info_rect.h + 128.f + 16.f,
            128.f, angle, angle + static_cast<float>(percent_psychics),
            color_table[2]);
        angle += static_cast<float>(percent_psychics);

        GPU_ArcFilled(_instance._screenTarget, info_rect.x + 128.f, info_rect.y + info_rect.h + 128.f + 16.f,
            128.f, angle, angle + static_cast<float>(percent_render),
            color_table[3]);
        angle += static_cast<float>(percent_render);

        GPU_ArcFilled(_instance._screenTarget, info_rect.x + 128.f, info_rect.y + info_rect.h + 128.f + 16.f,
            128.f, angle, angle + static_cast<float>(percent_post_process),
            color_table[4]);
        angle += static_cast<float>(percent_post_process);

        GPU_ArcFilled(_instance._screenTarget, info_rect.x + 128.f, info_rect.y + info_rect.h + 128.f + 16.f,
            128.f, angle, angle + static_cast<float>(percent_gpu_flip),
            color_table[5]);
        angle += static_cast<float>(percent_gpu_flip);

        GPU_ArcFilled(_instance._screenTarget, info_rect.x + 128.f, info_rect.y + info_rect.h + 128.f + 16.f,
            128.f, angle, angle + static_cast<float>(percent_other),
            color_table[6]);



        GPU_RectangleFilled2(_instance._screenTarget, info_rect, C_BLACK);
        GPU_Rectangle2(_instance._screenTarget, info_rect, C_DGREEN);

        //FC_DrawColor(_instance._global_font, _instance._screenTarget, info_rect.x + 4.f, info_rect.y + 4.f, C_DGREEN, text_left.c_str());
        float current_height = info_rect.y;
        Func::str_vec spliced_text = Func::Split(text_left, '\n');
        int i = 0;
    	for (std::string& text : spliced_text)
        {
            current_height += FC_DrawColor(_instance._global_font, _instance._screenTarget, info_rect.x + 4.f, current_height + 4.f, color_table[i++], text.c_str()).h;
        }

        FC_DrawColor(_instance._global_font, _instance._screenTarget, info_rect.x + info_rect.w - info_rect_right.w - 8.f, info_rect.y + 4.f, C_DGREEN, text_right.c_str(),
            _performance_counter_all,
            _performance_counter_step,
            _performance_counter_psychics,
            _performance_counter_render,
            _performance_counter_post_process,
            _performance_counter_gpu_flip,
            _performance_counter_other);
    }
}
#endif