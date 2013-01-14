#ifdef EMSCRIPTEN_NATIVE
#include "emscripten-qt-sdl.h"
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
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


    void (*attemptedLocalEventCallback)() = NULL;

    class WatchdogThread
    {
    public:
        WatchdogThread();
        void start();
        void notifyStartedWaitingForEvent();
        void notifyFinishedWaitingForEvent();
        void stop();
        static const int watchdogTimeoutMS = 2000;
    private:
        void watchdogLoop();
        bool stopRequested();
        SDL_cond *m_statusCondition;
        SDL_mutex *m_statusMutex;
        SDL_Thread *m_thread;
        bool m_waitingForEvent;
        bool m_stopRequested;
        static int startWatchdogLoop(void* watchDogPtr);
    } watchdogThread;
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

	int EMSCRIPTENQT_canvas_width_pixels()
	{
		return canvasWidthPixels;
	}
	int EMSCRIPTENQT_canvas_height_pixels()
	{
		return canvasHeightPixels;
	}
	int EMSCRIPTENQT_flush_pixels(uchar* data, int regionX, int regionY, int regionW, int regionH)
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
	void EMSCRIPTENQT_cursorChanged(int cursor)
	{
        qDebug() << "Cursor changed to " << cursor;
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
	switch (sdlKey)
	{
    case SDLK_RETURN:
        return Qt::Key_Enter;
	case SDLK_BACKSPACE:
		return Qt::Key_Backspace;
	case SDLK_LEFT:
		return Qt::Key_Left;
	case SDLK_RIGHT:
		return Qt::Key_Right;
	case SDLK_UP:
		return Qt::Key_Up;
	case SDLK_DOWN:
		return Qt::Key_Down;
    case SDLK_RSHIFT:
    case SDLK_LSHIFT:
        return Qt::Key_Shift;
    case SDLK_RCTRL:
    case SDLK_LCTRL:
        return Qt::Key_Control;
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
    if ((sdlMod & KMOD_RCTRL) != 0 || (sdlMod & KMOD_LCTRL) != 0)
    {
        qtModifiers = qtModifiers | Qt::ControlModifier;
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
        : m_statusMutex(SDL_CreateMutex()),
          m_statusCondition(SDL_CreateCond()),
          m_thread(NULL),
          m_stopRequested(false),
          m_waitingForEvent(false)
    {
    }

    void WatchdogThread::start()
    {
        m_thread = SDL_CreateThread(startWatchdogLoop, static_cast<void*>(this));
    }

    bool WatchdogThread::stopRequested()
    {
        SDL_mutexP(m_statusMutex);
        const bool result = m_stopRequested;
        SDL_mutexV(m_statusMutex);
        return result;
    }

    void WatchdogThread::stop()
    {
        SDL_mutexP(m_statusMutex);
        m_stopRequested = true;
        SDL_mutexV(m_statusMutex);
        SDL_CondSignal(m_statusCondition);
        SDL_WaitThread(m_thread, NULL);
    }

    void WatchdogThread::watchdogLoop()
    {
        static Uint32 lastTimeWaitingForEvent = 0;
        bool currentStarvationWasReported = false;
        Uint32 currentEventLoopStarvationBegin = 0;
        SDL_mutexP(m_statusMutex);
        while (!stopRequested())
        {
            if (m_waitingForEvent)
            {
                // Yawn ... just sleep until something interesting happens.
                SDL_CondWait(m_statusCondition, m_statusMutex);
            }
            else
            {
                if (!currentStarvationWasReported)
                {
                    currentEventLoopStarvationBegin = SDL_GetTicks();
                }
                SDL_CondWaitTimeout(m_statusCondition, m_statusMutex, watchdogTimeoutMS);
                if (!m_waitingForEvent)
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
        SDL_mutexV(m_statusMutex);
    }

    void WatchdogThread::notifyStartedWaitingForEvent()
    {
        SDL_mutexP(m_statusMutex);
        m_waitingForEvent = true;
        SDL_mutexV(m_statusMutex);
        SDL_CondSignal(m_statusCondition);
    }

    void WatchdogThread::notifyFinishedWaitingForEvent()
    {
        SDL_mutexP(m_statusMutex);
        m_waitingForEvent = false;
        SDL_mutexV(m_statusMutex);
        SDL_CondSignal(m_statusCondition);
    }

    int WatchdogThread::startWatchdogLoop(void* watchDogPtr)
    {
        static_cast<WatchdogThread*>(watchDogPtr)->watchdogLoop();
        qDebug() << "Watchdog thread exited";
        return 0;
    }
}

bool EmscriptenQtSDL::initScreen(int canvasWidthPixels, int canvasHeightPixels)
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
    // We also init the Watchdog, as we should expose any bits of code that take too long to
    // run as soon as possible.
    watchdogThread.start();
	return true;
}

int EmscriptenQtSDL::exec()
{
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
			const SDLKey sdlKey = event.key.keysym.sym;
			const bool isPress = (event.type == SDL_KEYDOWN);
            int unicode = event.key.keysym.unicode;
            if (!isPress && unicode == 0 && sdlKey <= 0xFF)
            {
                // For some reason, SDL neglects to inform us of the unicode when the key is released:
                // it's usually the same as the sdlKey, though.
                unicode = sdlKey;
            }
			const Qt::Key qtKey = sdlToQtKey(sdlKey);
			Qt::KeyboardModifiers qtModifiers = sdlModifiersToQtModifiers(event.key.keysym.mod);
            if (qtKey == Qt::Key_Shift && isPress)
            {
                qtModifiers |= Qt::ShiftModifier;
            }
            if (qtKey == Qt::Key_Control && isPress)
            {
                qtModifiers |= Qt::ControlModifier;
            }
			EMSCRIPTENQT_canvasKeyChanged(unicode, qtKey, qtModifiers, isPress, false);
		}

	}
	qDebug() << "Waiting for watchdogThread to terminate...";
    watchdogThread.stop();
	qDebug() << "Exiting SDL::exec";
	return 0;
}

void EmscriptenQtSDL::setAttemptedLocalEventLoopCallback(void(*callback)() )
{
    attemptedLocalEventCallback = callback;
}
extern int emscriptenQtSDLMain(int argc, char** argv);
int EmscriptenQtSDL::run(int canvasWidthPixels, int canvasHeightPixels, int argc, char** argv)
{
   initScreen(canvasWidthPixels, canvasHeightPixels);
   const int runEmscriptenQtSDLMainValue = emscriptenQtSDLMain(argc, argv);
   if (QCoreApplication::instance() == 0)
   {
       qWarning() << "No QApplication detected - did you stop it being deleted when emscriptenQtSDLMain(...) ended?";
   }
   if (runEmscriptenQtSDLMainValue != EXIT_SUCCESS)
   {
       return runEmscriptenQtSDLMainValue;
   }
   return exec();
}



#endif
