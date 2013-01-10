#ifdef EMSCRIPTEN_NATIVE
#include "emscripten-canvas-sdl.h"
#include <QtCore/QDebug>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_mutex.h>

namespace
{
	int canvasWidthPixels = 800;
	int canvasHeightPixels = 640;
	SDL_Surface *sdlCanvas = NULL;
	SDL_TimerID callbackTimer = NULL;
	bool sdlInited = false;

    const int watchdogTimeoutMS = 2000;

    void (*attemptedLocalEventCallback)() = NULL;

    class WatchdogThread
    {
    public:
        WatchdogThread();
        void start();
        void notifyStartedWaitingForEvent();
        void notifyFinishedWaitingForEvent();
        void stop();
    private:
        void watchdogLoop();
        bool stopRequested();
        SDL_cond *watchDogStatusCondition;
        SDL_mutex *watchDogStatusMutex;
        SDL_Thread *watchdogThread;
        bool waitingForEvent;
        bool m_stopRequested;
        static int startWatchdogLoop(void* watchDogPtr);
    };
}

extern "C" 
{
	void EMSCRIPTENQT_mouseCanvasPosChanged(int x, int y); 
	void EMSCRIPTENQT_mouseCanvasButtonChanged(int button, int state);
	void EMSCRIPTENQT_canvasKeyChanged(int unicode, int keycode, int modifiers, int isPress, int autoRepeat);


	void setpixel(SDL_Surface *screen, int x, int y, Uint8 r, Uint8 g, Uint8 b)
	{

		Uint32 colour = SDL_MapRGB( screen->format, r, g, b );

		Uint32 *pixmem32 = (Uint32*) screen->pixels  + y + x;
		*pixmem32 = colour;
	}

	Uint32 timerCallback(Uint32 interval, void *param)
	{
		SDL_Event event;
		SDL_UserEvent userevent;

		userevent.type = SDL_USEREVENT;
		userevent.code = 0;
		userevent.data1 = NULL;
		userevent.data2 = NULL;

		event.type = SDL_USEREVENT;
		event.user = userevent;

		SDL_PushEvent(&event);
		return 0; // We don't want this timer to repeat, thanks!
	}

	void EMSCRIPTENQT_resetTimerCallback(long milliseconds)
	{
		if (!sdlInited)
		{	
			// Can't set SDL timers before SDL is inited; ignore.
			return;
		}
		if (callbackTimer)
		{
			SDL_RemoveTimer(callbackTimer);
			callbackTimer = NULL;
		}
		if (milliseconds == 0)
		{
			milliseconds = 1;
		}
		callbackTimer = SDL_AddTimer(milliseconds, timerCallback, NULL);
		if (callbackTimer == NULL)
		{
			qDebug() << "Error adding SDL timer: " << SDL_GetError();
		}
	}

	void EMSCRIPTENQT_timerCallback();

	int EMSCRIPTEN_canvas_width_pixels()
	{
		return canvasWidthPixels;
	}
	int EMSCRIPTEN_canvas_height_pixels()
	{
		return canvasHeightPixels;
	}
	int EMSCRIPTEN_flush_pixels(uchar* data, int regionX, int regionY, int regionW, int regionH)
	{
		int x, y, ytimesw;
		const int BPP = 4;

		if(SDL_MUSTLOCK(sdlCanvas)) 
		{
			if(SDL_LockSurface(sdlCanvas) < 0) return -1;
		}

		uchar *pos = data + 4 * (regionY * canvasWidthPixels + regionX);
		for(y = regionY; y < regionY + regionH; y++ )
		{
			ytimesw = y*sdlCanvas->pitch/BPP;
			for( x = regionX; x < regionX + regionW; x++ )
			{
				const int b = *pos;
				pos++;
				const int g = *pos;
				pos++;
				const int r = *pos;
				pos++;
				pos++;
				setpixel(sdlCanvas, x, ytimesw, r, g, b);
			}
            pos += 4 * (canvasWidthPixels - regionW);
		}

		if(SDL_MUSTLOCK(sdlCanvas)) SDL_UnlockSurface(sdlCanvas);

		SDL_Flip(sdlCanvas); 
		return 0;
	}
	void EMSCRIPTENQT_cursorChanged(int)
	{
	}

	void EMSCRIPTENQT_attemptedLocalEventLoop()
    {
        if (attemptedLocalEventCallback)
        {
            attemptedLocalEventCallback();
        }
    }
}

Qt::Key sdlToQtKey(SDLKey sdlKey)
{
	if (sdlKey >= SDLK_a && sdlKey <= SDLK_z)
	{
		return static_cast<Qt::Key>((sdlKey - SDLK_a) + Qt::Key_A);
	}
	switch (sdlKey)
	{
	case SDLK_BACKSPACE:
		return Qt::Key_Backspace;
	case SDLK_LEFT:
		qDebug() << "Left";
		return Qt::Key_Left;
	case SDLK_RIGHT:
		return Qt::Key_Right;
	case SDLK_UP:
		return Qt::Key_Up;
	case SDLK_DOWN:
		return Qt::Key_Down;
	default:
		return static_cast<Qt::Key>(0);
	}
}

Qt::KeyboardModifiers sdlModifiersToQtModifiers(SDLMod sdlMod)
{
	Qt::KeyboardModifiers qtModifiers = Qt::NoModifier;
	if ((sdlMod & KMOD_RSHIFT) != 0 || (sdlMod & KMOD_LSHIFT) != 0)
	{
		qtModifiers = qtModifiers | Qt::ShiftModifier;
	}
	return qtModifiers;
}

Qt::MouseButton sdlButtonToQtButton(Uint8 sdlButton)
{
    switch (sdlButton)
    {
        case SDL_BUTTON_LEFT:
            return Qt::LeftButton;
        case SDL_BUTTON_RIGHT:
            return Qt::RightButton;
        case SDL_BUTTON_MIDDLE:
            return Qt::MiddleButton;
    }
    qWarning() << "Did not recognise SDL mouse button: " << sdlButton;
    return Qt::NoButton;
}

namespace
{
    WatchdogThread::WatchdogThread()
        : watchDogStatusMutex(SDL_CreateMutex()),
          watchDogStatusCondition(SDL_CreateCond()),
          m_stopRequested(false),
          waitingForEvent(false)
    {
    }

    void WatchdogThread::start()
    {
        watchdogThread = SDL_CreateThread(startWatchdogLoop, static_cast<void*>(this));
    }

    bool WatchdogThread::stopRequested()
    {
        SDL_mutexP(watchDogStatusMutex);
        const bool result = m_stopRequested;
        SDL_mutexV(watchDogStatusMutex);
        return result;
    }

    void WatchdogThread::stop()
    {
        SDL_mutexP(watchDogStatusMutex);
        m_stopRequested = true;
        SDL_mutexV(watchDogStatusMutex);
        SDL_CondSignal(watchDogStatusCondition);
    }

    void WatchdogThread::watchdogLoop()
    {
        static Uint32 lastTimeWaitingForEvent = 0;
        bool currentStarvationWasReported = false;
        Uint32 currentEventLoopStarvationBegin = 0;
        SDL_mutexP(watchDogStatusMutex);
        while (!stopRequested())
        {
            if (waitingForEvent)
            {
                // Yawn ... just sleep until something interesting happens.
                SDL_CondWait(watchDogStatusCondition, watchDogStatusMutex);
            }
            else
            {
                if (!currentStarvationWasReported)
                {
                    currentEventLoopStarvationBegin = SDL_GetTicks();
                }
                SDL_CondWaitTimeout(watchDogStatusCondition, watchDogStatusMutex, watchdogTimeoutMS);
                if (!waitingForEvent)
                {
                    // We weren't woken up to be informed that we are now waiting for an event:
                    // therefore, we are (likely) still starving the event loop, although we could have been
                    // woken to report a quit event.  Best check just in case.
                    const Uint32 currentStarvationTimeMS = SDL_GetTicks() - currentEventLoopStarvationBegin;
                    if (currentStarvationTimeMS >= watchdogTimeoutMS)
                    {
                        qDebug() << "** Warning ** Event loop has been starved for " << currentStarvationTimeMS << "ms";
                        currentStarvationWasReported = true;
                    }
                }
                else
                {
                    // Starvation is over!
                    if (currentStarvationWasReported)
                    {
                        const Uint32 currentStarvationTimeMS = SDL_GetTicks() - currentEventLoopStarvationBegin;
                        qDebug() << "Starvation has ceased; it lasted for " << currentStarvationTimeMS << "ms";
                    }
                    currentStarvationWasReported = false;
                }
            }
        }
        SDL_mutexV(watchDogStatusMutex);
    }

    void WatchdogThread::notifyStartedWaitingForEvent()
    {
        SDL_mutexP(watchDogStatusMutex);
        waitingForEvent = true;
        SDL_mutexV(watchDogStatusMutex);
        SDL_CondSignal(watchDogStatusCondition);
    }

    void WatchdogThread::notifyFinishedWaitingForEvent()
    {
        SDL_mutexP(watchDogStatusMutex);
        waitingForEvent = false;
        SDL_mutexV(watchDogStatusMutex);
        SDL_CondSignal(watchDogStatusCondition);
    }

    int WatchdogThread::startWatchdogLoop(void* watchDogPtr)
    {
        static_cast<WatchdogThread*>(watchDogPtr)->watchdogLoop();
        return 0;
    }
}

bool EmscriptenSDL::initScreen(int canvasWidthPixels, int canvasHeightPixels)
{
	::canvasWidthPixels = canvasWidthPixels;
	::canvasHeightPixels = canvasHeightPixels;
	if (SDL_Init( SDL_INIT_EVERYTHING ) == -1)
	{
		qDebug() << "SDL init failed!";
		return false;
	}
	sdlCanvas = SDL_SetVideoMode( canvasWidthPixels, canvasHeightPixels, 32, SDL_SWSURFACE );
	if (!sdlCanvas)
	{
		qDebug() << "Creation of " << canvasWidthPixels << "x" << canvasHeightPixels << " SDL surface failed!";
		return false;
	}
	sdlInited = true;
	return true;
}

int EmscriptenSDL::exec()
{
    WatchdogThread watchdogThread;
    watchdogThread.start();

	qDebug() << "SDL - woo!";
	// Any requested timers from before we called SDL_Init would have been ignored, so let's
	// set up a synthetic one, now.
	EMSCRIPTENQT_resetTimerCallback(0);

	SDL_Event event;
	SDL_EnableUNICODE( 1 );
    bool quit = false;
	while (!quit)
	{
        watchdogThread.notifyStartedWaitingForEvent();
		SDL_WaitEvent(&event);
        watchdogThread.notifyFinishedWaitingForEvent();
		if( event.type == SDL_QUIT )
		{
			qDebug() << "Quitting";
            quit = true;
		}    
		else if (event.type == SDL_USEREVENT)
		{
			// Qt timer.
			EMSCRIPTENQT_timerCallback();
		}
		else if (event.type == SDL_MOUSEMOTION)
		{
			EMSCRIPTENQT_mouseCanvasPosChanged(event.motion.x, event.motion.y);
		}
		else if (event.type == SDL_MOUSEBUTTONUP)
		{
			EMSCRIPTENQT_mouseCanvasButtonChanged(sdlButtonToQtButton(event.button.button), 0);
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN)
		{
            EMSCRIPTENQT_mouseCanvasButtonChanged(sdlButtonToQtButton(event.button.button), 1);
		}
		else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
		{
			const int unicode = event.key.keysym.unicode;
			const SDLKey sdlKey = event.key.keysym.sym;
			const bool isPress = (event.type == SDL_KEYDOWN);
			const Qt::Key qtKey = sdlToQtKey(sdlKey);
			const Qt::KeyboardModifiers qtModifiers = sdlModifiersToQtModifiers(event.key.keysym.mod);
			EMSCRIPTENQT_canvasKeyChanged(unicode, qtKey, qtModifiers, isPress, false);
		}

	}
	qDebug() << "Waiting for watchdogThread to terminate...";
    watchdogThread.stop();
	qDebug() << "Exiting SDL::exec";
	return 0;
}

void EmscriptenSDL::setAttemptedLocalEventLoopCallback(void(*callback)() )
{
    attemptedLocalEventCallback = callback;
}


#endif
