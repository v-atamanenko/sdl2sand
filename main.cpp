#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
/*
 *  SDL2Sand
 *
 *  Copyright © 2006 Thomas RenÈ Sidor, Kristian Jensen
 *  Copyright © 2014 Artur Rojek
 *  Copyright © 2022 Volodymyr Atamanenko
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "SDL.h"

#include "CmdLine.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define FASTRAND_MAX 32767
static unsigned int g_seed;

inline void fast_srand( int seed ) {
    g_seed = seed;
}

inline int fastrand() {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

//The application time based timer
class LTimer
{
public:
    //Initializes variables
    LTimer();

    //The various clock actions
    void start();
    void stop();
    void pause();
    void unpause();

    //Gets the timer's time
    Uint32 getTicks();

    //Checks the status of the timer
    bool isStarted();
    bool isPaused();

private:
    //The clock time when the timer started
    Uint32 mStartTicks;

    //The ticks stored when the timer was paused
    Uint32 mPausedTicks;

    //The timer status
    bool mPaused;
    bool mStarted;
};

LTimer::LTimer()
{
    //Initialize the variables
    mStartTicks = 0;
    mPausedTicks = 0;

    mPaused = false;
    mStarted = false;
}

void LTimer::start()
{
    //Start the timer
    mStarted = true;

    //Unpause the timer
    mPaused = false;

    //Get the current clock time
    mStartTicks = SDL_GetTicks();
    mPausedTicks = 0;
}

void LTimer::stop()
{
    //Stop the timer
    mStarted = false;

    //Unpause the timer
    mPaused = false;

    //Clear tick variables
    mStartTicks = 0;
    mPausedTicks = 0;
}

void LTimer::pause()
{
    //If the timer is running and isn't already paused
    if( mStarted && !mPaused )
    {
        //Pause the timer
        mPaused = true;

        //Calculate the paused ticks
        mPausedTicks = SDL_GetTicks() - mStartTicks;
        mStartTicks = 0;
    }
}

void LTimer::unpause()
{
    //If the timer is running and paused
    if( mStarted && mPaused )
    {
        //Unpause the timer
        mPaused = false;

        //Reset the starting ticks
        mStartTicks = SDL_GetTicks() - mPausedTicks;

        //Reset the paused ticks
        mPausedTicks = 0;
    }
}

Uint32 LTimer::getTicks()
{
    //The actual timer time
    Uint32 time = 0;

    //If the timer is running
    if( mStarted )
    {
        //If the timer is paused
        if( mPaused )
        {
            //Return the number of ticks when the timer was paused
            time = mPausedTicks;
        }
        else
        {
            //Return the current time minus the start time
            time = SDL_GetTicks() - mStartTicks;
        }
    }

    return time;
}

bool LTimer::isStarted()
{
    //Timer is running and paused or unpaused
    return mStarted;
}

bool LTimer::isPaused()
{
    //Timer is running and paused
    return mPaused && mStarted;
}


int JOY_DEADZONE = 500;

//Screen size
int WIDTH;
int HEIGHT;

// FPS
const int SCREEN_FPS = 30;
const int SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

//Button sizes
int BUTTON_SIZE = 10;
int UPPER_ROW_Y;
int MIDDLE_ROW_Y;
int LOWER_ROW_Y;

const int DASHBOARD_HEIGHT = BUTTON_SIZE + 2;

int particleCount = 0;
int penSize = 2;

int mbx;
int mby;

int canMoveX = 0;
int canMoveY = 0;

int speedX = 0;
int speedY = 0;

//Init and declare ParticleSwapping
bool implementParticleSwaps = true;



/* 
Enumerating conventions
-----------------------
Stillborn: between STILLBORN_UPPER_BOUND and STILLBORN_LOWER_BOUND
Floating: between FLOATING_UPPER_BOUND and FLOATING_LOWER_BOUND
*/
const int STILLBORN_UPPER_BOUND = 14;
const int STILLBORN_LOWER_BOUND = 1;
const int FLOATING_UPPER_BOUND = 35;
const int FLOATING_LOWER_BOUND = 32;

enum ParticleType
{
    // STILLBORN
    NOTHING = 0,
    WALL = 1,
    IRONWALL = 2,
    TORCH = 3,
    //x = 4,
    STOVE = 5,
    ICE = 6,
    RUST = 7,
    EMBER = 8,
    PLANT = 9,
    VOID = 10,

    //SPOUTS
    WATERSPOUT = 11,
    SANDSPOUT = 12,
    SALTSPOUT = 13,
    OILSPOUT = 14,
    //x = 15,

    //ELEMENTAL
    WATER = 16,
    MOVEDWATER = 17,
    DIRT = 18,
    MOVEDDIRT = 19,
    SALT = 20,
    MOVEDSALT = 21,
    OIL = 22,
    MOVEDOIL = 23,
    SAND = 24,
    MOVEDSAND = 25,

    //COMBINED
    SALTWATER = 26,
    MOVEDSALTWATER = 27,
    MUD = 28,
    MOVEDMUD = 29,
    ACID = 30,
    MOVEDACID = 31,

    //FLOATING
    STEAM = 32,
    MOVEDSTEAM = 33,
    FIRE = 34,
    MOVEDFIRE = 35,

    //ELECTRICITY
    ELEC = 36,
    MOVEDELEC = 37
};

// Instead of using a two-dimensional array
// we'll use a simple array to improve speed
// vs = virtual screen
ParticleType *vs;

// The current brush type
ParticleType CurrentParticleType = WALL;
ParticleType LastParticleType = NOTHING;

//The number of buttons
const int BUTTON_COUNT = 19;

// Button rectangle struct
typedef struct
{
    SDL_Rect rect;
    ParticleType particleType;
} ButtonRect;

//An array of the buttons
ButtonRect Button[BUTTON_COUNT];

// The particle system play area
SDL_Rect scene;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *scene_texture;
uint32_t *screen_buffer;

//Checks wether a given particle type is a stillborn element
static inline bool IsStillborn(ParticleType t)
{
    return (t >= STILLBORN_LOWER_BOUND && t <= STILLBORN_UPPER_BOUND);
}

//Checks wether a given particle type is a floting type - like FIRE and STEAM
static inline bool IsFloating(ParticleType t)
{
    return (t >= FLOATING_LOWER_BOUND && t <= FLOATING_UPPER_BOUND);
}

//Checks wether a given particle type is burnable - like PLANT and OIL
static inline bool IsBurnable(ParticleType t)
{
    return (t == PLANT || t == OIL || t == MOVEDOIL);
}

//Checks wether a given particle type is burnable - like PLANT and OIL
static inline bool BurnsAsEmber(ParticleType t)
{
    return (t == PLANT); //Maybe we'll add a FUSE or WOOD
}

std::map<ParticleType, SDL_Color> colors;

// Initializing colors
void initColors()
{
    //STILLBORN
    colors[SAND]		= { 238, 204, 128, 255};
    colors[WALL]		= { 100, 100, 100, 255};
    colors[VOID]		= { 60, 60, 60, 255};
    colors[IRONWALL]	= { 110, 110, 110, 255};
    colors[TORCH]		= { 139, 69, 19, 255};
    colors[STOVE]		= { 74, 74, 74, 255};
    colors[ICE]			= { 175, 238, 238, 255};
    colors[PLANT]		= { 0, 150, 0, 255};
    colors[EMBER]		= { 127, 25, 25, 255};
    colors[RUST]		= { 110, 40, 10, 255};

    //ELEMENTAL
    colors[WATER]		= { 32, 32, 255, 255};
    colors[DIRT]		= { 205, 175, 149, 255};
    colors[SALT]		= { 255, 255, 255, 255};
    colors[OIL]			= { 128, 64, 64, 255};

    //COMBINED
    colors[MUD]			= { 139, 69, 19, 255};
    colors[SALTWATER]	= { 65, 105, 225, 255};
    colors[STEAM]		= { 95, 158, 160, 255};

    //EXTRA
    colors[ACID]		= { 173, 255, 47, 255};
    colors[FIRE]		= { 255, 50, 50, 255};
    colors[ELEC]		= { 255, 255, 0, 255};

    //SPOUTS
    colors[WATERSPOUT]	= { 0, 0, 128, 255};
    colors[SANDSPOUT]	= { 240, 230, 140, 255};
    colors[SALTSPOUT]	= { 238, 233, 233, 255};
    colors[OILSPOUT]	= { 108, 44, 44, 255};
}

//Drawing our virtual screen to the real screen
static void DrawScene()
{
    particleCount = 0;

    size_t framebuf_size = scene.w * scene.h * 3 * sizeof(Uint8);
    auto* pixels = static_cast<Uint8 *>(malloc(framebuf_size));
    memset(pixels, 0, framebuf_size);

    //Iterating through each pixel height first
    for(int y=scene.h;y--;)
    {
        //Width
        for(int x=scene.w;x--;)
        {
            const unsigned int offset = ( scene.w * 3 * y ) + x * 3;
            int index = x+(scene.w*y);
            ParticleType same = vs[index];
            if(same != NOTHING)
            {
                if(IsStillborn(same)) {

                    pixels[ offset + 0 ] = colors[same].r;
                    pixels[ offset + 1 ] = colors[same].g;
                    pixels[ offset + 2 ] = colors[same].b;
                } else
                {
                    particleCount++;
                    if(same % 2 == 1) //Moved
                    {
                        pixels[ offset + 0 ] = colors[(ParticleType)(same-1)].r;
                        pixels[ offset + 1 ] = colors[(ParticleType)(same-1)].g;
                        pixels[ offset + 2 ] = colors[(ParticleType)(same-1)].b;
                        vs[index] = (ParticleType)(same-1); //Set it to not moved
                    }
                    else {			//Not moved
                        pixels[ offset + 0 ] = colors[same].r;
                        pixels[ offset + 1 ] = colors[same].g;
                        pixels[ offset + 2 ] = colors[same].b;
                    }
                }
            }
        }
    }

    SDL_UpdateTexture(scene_texture, nullptr, pixels, scene.w * 3);
    free(pixels);
    SDL_RenderCopy(renderer, scene_texture, nullptr, &scene);
}

// Emitting a given particletype at (x,o) width pixels wide and
// with a p density (probability that a given pixel will be drawn
// at a given position withing the width)
void Emit(int x, int width, ParticleType type, float p)
{
    for (int i = x - width/2; i < x + width/2; i++)
    {
        if ( fastrand() < (int)(FASTRAND_MAX * p) ) vs[i+WIDTH] = type;
    }
}

//Performs logic of stillborn particles
void StillbornParticleLogic(int x,int y,ParticleType type)
{
    int index, above, left, right, below, same, abovetwo;
    switch(type)
    {

        case VOID:
            above = x+((y-1)*WIDTH);
            left = (x+1)+(y*WIDTH);
            right = (x-1)+(y*WIDTH);
            below = x+((y+1)*WIDTH);
            if(vs[above] != NOTHING)
                vs[above] = NOTHING;
            if(vs[below] != NOTHING)
                vs[below] = NOTHING;
            if(vs[left] != NOTHING)
                vs[left] = NOTHING;
            if(vs[right] != NOTHING)
                vs[right] = NOTHING;
            break;
        case IRONWALL:
            above = x+((y-1)*WIDTH);
            left = (x+1)+(y*WIDTH);
            right = (x-1)+(y*WIDTH);
            if(fastrand()%200 == 0 && (vs[above] == RUST || vs[left] == RUST || vs[right] == RUST))
                vs[x+(y*WIDTH)] = RUST;
            break;
        case TORCH:
            above = x+((y-1)*WIDTH);
            left = (x+1)+(y*WIDTH);
            right = (x-1)+(y*WIDTH);
            if(fastrand()%2 == 0) // Spawns fire
            {
                if(vs[above] == NOTHING || vs[above] == MOVEDFIRE) //Fire above
                    vs[above] = MOVEDFIRE;
                if(vs[right] == NOTHING || vs[right] == MOVEDFIRE) //Fire to the right
                    vs[right] = MOVEDFIRE;
                if(vs[left] == NOTHING || vs[left] == MOVEDFIRE) //Fire to the left
                    vs[left] = MOVEDFIRE;
            }
            if(vs[above] == MOVEDWATER || vs[above] == WATER) //Fire above
                vs[above] = MOVEDSTEAM;
            if(vs[right] == MOVEDWATER || vs[right] == WATER) //Fire to the right
                vs[right] = MOVEDSTEAM;
            if(vs[left] == MOVEDWATER || vs[left] == WATER) //Fire to the left
                vs[left] = MOVEDSTEAM;

            break;
        case PLANT:
            if(fastrand()%2 == 0) //Making the plant grow slowly
            {
                index = 0;
                switch(fastrand()%4)
                {
                    case 0: index = (x-1)+(y*WIDTH); break;
                    case 1: index = x+((y-1)*WIDTH); break;
                    case 2: index = (x+1)+(y*WIDTH); break;
                    case 3:	index = x+((y+1)*WIDTH); break;
                }
                if(vs[index] == WATER)
                    vs[index] = PLANT;
            }
            break;
        case EMBER:
            below = x+((y+1)*WIDTH);
            if(vs[below] == NOTHING || IsBurnable(vs[below]))
                vs[below] = FIRE;

            index = 0;
            switch(fastrand()%4)
            {
                case 0: index = (x-1)+(y*WIDTH); break;
                case 1: index = x+((y-1)*WIDTH); break;
                case 2: index = (x+1)+(y*WIDTH); break;
                case 3:	index = x+((y+1)*WIDTH); break;
            }
            if(vs[index] == PLANT)
                vs[index] = FIRE;

            if(fastrand()%18 == 0) // Making ember burn out slowly
                vs[x+(y*WIDTH)] = NOTHING;
            break;
        case STOVE:
            above = x+((y-1)*WIDTH);
            abovetwo = x+((y-2)*WIDTH);
            if(fastrand()%4 == 0 && vs[above] == WATER) // Boil the water
                vs[above] = STEAM;
            if(fastrand()%4 == 0 && vs[above] == SALTWATER) // Saltwater separates
            {
                vs[above] = SALT;
                vs[abovetwo] = STEAM;
            }
            if(fastrand()%8 == 0 && vs[above] == OIL) // Set oil aflame
                vs[above] = EMBER;
            break;
        case RUST:
            if(fastrand()%7000 == 0)//Deteriate rust
                vs[x+(y*WIDTH)] = NOTHING;
            break;


            //####################### SPOUTS #######################
        case WATERSPOUT:
            if(fastrand()%6 == 0) // Take it easy on the spout
            {
                below = x+((y+1)*WIDTH);
                if (vs[below] == NOTHING)
                    vs[below] = MOVEDWATER;
            }
            break;
        case SANDSPOUT:
            if(fastrand()%6 == 0) // Take it easy on the spout
            {
                below = x+((y+1)*WIDTH);
                if (vs[below] == NOTHING)
                    vs[below] = MOVEDSAND;
            }
            break;
        case SALTSPOUT:
            if(fastrand()%6 == 0) // Take it easy on the spout
            {

                below = x+((y+1)*WIDTH);
                if (vs[below] == NOTHING)
                    vs[below] = MOVEDSALT;
                if(vs[below] == WATER || vs[below] == MOVEDWATER)
                    vs[below] = MOVEDSALTWATER;
            }
            break;
        case OILSPOUT:
            if(fastrand()%6 == 0) // Take it easy on the spout
            {
                below = x+((y+1)*WIDTH);
                if (vs[below] == NOTHING)
                    vs[below] = MOVEDOIL;
            }
            break;

        default:
            break;
    }

}

// Performing the movement logic of a given particle. The argument 'type'
// is passed so that we don't need a table lookup when determining the
// type to set the given particle to - i.e. if the particle is SAND then the
// passed type will be MOVEDSAND
inline void MoveParticle(int x, int y, ParticleType type)
{

    type = (ParticleType)(type+1);


    int above = x+((y-1)*WIDTH);
    int same = x+(WIDTH*y);
    int below = x+((y+1)*WIDTH);


    //If nothing below then just fall (gravity)
    if(!IsFloating(type))
    {
        if ( (vs[below] == NOTHING) && (fastrand() % 8)) //fastrand() % 8 makes it spread
        {
            vs[below] = type;
            vs[same] = NOTHING;
            return;
        }
    }
    else
    {
        if(fastrand()%3 == 0) //Slow down please
            return;

        //If nothing above then rise (floating - or reverse gravity? ;))
        if ((vs[above] == NOTHING || vs[above] == FIRE) && (fastrand() % 8) && (vs[same] != ELEC) && (vs[same] != MOVEDELEC)) //fastrand() % 8 makes it spread
        {
            if (type == MOVEDFIRE && fastrand()%20 == 0)
                vs[same] = NOTHING;
            else
            {
                vs[above] = vs[same];
                vs[same] = NOTHING;
            }
            return;
        }

    }

    //Randomly select right or left first
    int sign = fastrand() % 2 == 0 ? -1 : 1;

    // We'll only calculate these indicies once for optimization purpose
    int first = (x+sign)+(WIDTH*y);
    int second = (x-sign)+(WIDTH*y);

    int index = 0;
    //Particle type specific logic
    switch(type)
    {
        case MOVEDELEC:
            if(fastrand()%2 == 0)
                vs[same] = NOTHING;
            break;
        case MOVEDSTEAM:
            if(fastrand()%1000 == 0)
            {
                vs[same] = MOVEDWATER;
                return;
            }
            if(fastrand()%500 == 0)
            {
                vs[same] = NOTHING;
                return;
            }
            if(!IsStillborn(vs[above]) && !IsFloating(vs[above]))
            {
                if(fastrand()%15 == 0)
                {
                    vs[same] = NOTHING;
                    return;
                }
                else
                {
                    vs[same] = vs[above];
                    vs[above] = MOVEDSTEAM;
                    return;
                }
            }
            break;
        case MOVEDFIRE:

            if(!IsBurnable(vs[above]) && fastrand()%10 == 0)
            {
                vs[same] = NOTHING;
                return;
            }

            // Let the snowman melt!
            if(fastrand()%4 == 0)
            {
                if (vs[above] == ICE)
                {
                    vs[above] = WATER;
                    vs[same] = NOTHING;
                }
                if (vs[below] == ICE)
                {
                    vs[below] = WATER;
                    vs[same] = NOTHING;
                }
                if (vs[first] == ICE)
                {
                    vs[first] = WATER;
                    vs[same] = NOTHING;
                }
                if (vs[second] == ICE)
                {
                    vs[second] = WATER;
                    vs[same] = NOTHING;
                }
            }

            //Let's burn whatever we can!
            index = 0;
            switch(fastrand()%4)
            {
                case 0: index = above; break;
                case 1: index = below; break;
                case 2: index = first; break;
                case 3:	index = second; break;
            }
            if(IsBurnable(vs[index]))
            {
                if(BurnsAsEmber(vs[index]))
                    vs[index] = EMBER;
                else
                    vs[index] = FIRE;
            }
            break;
        case MOVEDWATER:
            if(fastrand()%200 == 0 && vs[below] == IRONWALL)
                vs[below] = RUST;

            if(vs[below]  == FIRE || vs[above] == FIRE || vs[first] == FIRE || vs[second] == FIRE)
                vs[same] = MOVEDSTEAM;

            //Making water+dirt into dirt
            if(vs[below] == DIRT)
            {
                vs[below] = MOVEDMUD;
                vs[same] = NOTHING;
            }
            if(vs[above] == DIRT)
            {
                vs[above] = MOVEDMUD;
                vs[same] = NOTHING;
            }

            //Making water+salt into saltwater
            if(vs[above] == SALT || vs[above] == MOVEDSALT)
            {
                vs[above] = MOVEDSALTWATER;
                vs[same] = NOTHING;
            }
            if(vs[below] == SALT || vs[below] == MOVEDSALT)
            {
                vs[below] = MOVEDSALTWATER;
                vs[same] = NOTHING;
            }

            if(fastrand()%60 == 0) //Melting ice
            {
                switch(fastrand()%4)
                {
                    case 0:	index = above; break;
                    case 1:	index = below; break;
                    case 2:	index = first; break;
                    case 3:	index = second; break;
                }
                if(vs[index] == ICE)vs[index] = WATER; //--
            }
            break;
        case MOVEDACID:
            switch(fastrand()%4)
            {
                case 0:	index = above; break;
                case 1:	index = below; break;
                case 2:	index = first; break;
                case 3:	index = second; break;
            }
            if(vs[index] != WALL && vs[index] != IRONWALL && vs[index] != WATER && vs[index] != MOVEDWATER && vs[index] != ACID && vs[index] != MOVEDACID) vs[index] = NOTHING;	break;
            break;
        case MOVEDSALT:
            if(fastrand()%20 == 0)
            {
                switch(fastrand()%4)
                {
                    case 0:	index = above; break;
                    case 1:	index = below; break;
                    case 2:	index = first; break;
                    case 3:	index = second; break;
                }
                if(vs[index] == ICE)vs[index] = WATER; //--
            }
            break;
        case MOVEDSALTWATER:
            //Saltwater separated by heat
            //	if (vs[above] == FIRE || vs[below] == FIRE || vs[first] == FIRE || vs[second] == FIRE || vs[above] == STOVE || vs[below] == STOVE || vs[first] == STOVE || vs[second] == STOVE)
            //	{
            //		vs[same] = SALT;
            //		vs[above] = STEAM;
            //	}
            if(fastrand()%40 == 0) //Saltwater dissolves ice more slowly than pure salt
            {
                switch(fastrand()%4)
                {
                    case 0:	index = above; break;
                    case 1:	index = below; break;
                    case 2:	index = first; break;
                    case 3:	index = second; break;
                }
                if(vs[index] == ICE)vs[index] = WATER;
            }
            break;
        case MOVEDOIL:
            switch(fastrand()%4)
            {
                case 0:	index = above; break;
                case 1:	index = below; break;
                case 2:	index = first; break;
                case 3:	index = second; break;
            }
            if(vs[index] == FIRE)
                vs[same] = FIRE;
            break;

        default:
            break;
    }

    //Peform 'realism' logic?
    // When adding dynamics to this part please use the following structure:
    // If a particle A is ligther than particle B then add vs[above] == B to the condition in case A (case MOVED_A)
    if(implementParticleSwaps)
    {
        switch(type)
        {
            case MOVEDWATER:
                if(vs[above] == SAND || vs[above] == MUD || vs[above] == SALTWATER && fastrand()%3 == 0)
                {
                    vs[same] = vs[above];
                    vs[above] = type;
                    return;
                }
                break;
            case MOVEDOIL:
                if(vs[above] == WATER && fastrand()%3 == 0)
                {
                    vs[same] = vs[above];
                    vs[above] = type;
                    return;
                }
                break;
            case MOVEDSALTWATER:
                if(vs[above] == DIRT || vs[above] == MUD || vs[above] == SAND && fastrand()%3 == 0)
                {
                    vs[same] = vs[above];
                    vs[above] = type;
                    return;
                }
                break;

            default:
                break;
        }
    }

    // The place below (x,y+1) is filled with something, then check (x+sign,y+1) and (x-sign,y+1)
    // We chose sign randomly to randomly check eigther left or right
    // This is for elements that fall downward
    if (!IsFloating(type))
    {
        int firstdown = (x+sign)+((y+1)*WIDTH);
        int seconddown = (x-sign)+((y+1)*WIDTH);

        if ( vs[firstdown] == NOTHING)
        {
            vs[firstdown] = type;
            vs[same] = NOTHING;
        }
        else if ( vs[seconddown] == NOTHING)
        {
            vs[seconddown] = type;
            vs[same] = NOTHING;
        }
            //If (x+sign,y+1) is filled then try (x+sign,y) and (x-sign,y)
        else if (vs[first] == NOTHING)
        {
            vs[first] = type;
            vs[same] = NOTHING;
        }
        else if (vs[second] == NOTHING)
        {
            vs[second] = type;
            vs[same] = NOTHING;
        }
    }
        // Make steam move
    else if(type == MOVEDSTEAM)
    {
        int firstup = (x+sign)+((y-1)*WIDTH);
        int secondup = (x-sign)+((y-1)*WIDTH);

        if ( vs[firstup] == NOTHING)
        {
            vs[firstup] = type;
            vs[same] = NOTHING;
        }
        else if ( vs[secondup] == NOTHING)
        {
            vs[secondup] = type;
            vs[same] = NOTHING;
        }
            //If (x+sign,y+1) is filled then try (x+sign,y) and (x-sign,y)
        else if (vs[first] == NOTHING)
        {
            vs[first] = type;
            vs[same] = NOTHING;
        }
        else if (vs[second] == NOTHING)
        {
            vs[second] = type;
            vs[same] = NOTHING;
        }
    }
}

//Drawing a filled circle at a given position with a given radius and a given partice type
void DrawParticles(int xpos, int ypos, int radius, ParticleType type)
{
    for (int x = ((xpos - radius - 1) < 0) ? 0 : (xpos - radius - 1); x <= xpos + radius && x < WIDTH; x++) {
        for (int y = ((ypos - radius - 1) < 0) ? 0 : (ypos - radius - 1); y <= ypos + radius && y < HEIGHT; y++)
        {
            if ((x-xpos)*(x-xpos) + (y-ypos)*(y-ypos) <= radius*radius) vs[x+(WIDTH*y)] = type;
        }
    }
}

// Drawing a line
void DrawLine(int newx, int newy, int oldx, int oldy)
{
    if(newx == oldx && newy == oldy)
    {
        DrawParticles(newx,newy,penSize,CurrentParticleType);
    }
    else
    {
        float step = 1.0f / ((abs(newx-oldx)>abs(newy-oldy)) ? abs(newx-oldx) : abs(newy-oldy));
        for (float a = 0; a < 1; a+=step)
            DrawParticles(a*newx+(1-a)*oldx,a*newy+(1-a)*oldy,penSize,CurrentParticleType);
    }
}

// Updating a virtual pixel
inline void UpdateVirtualPixel(int x, int y)
{
    ParticleType same = vs[x+(WIDTH*y)];
    if(same != NOTHING)
    {
        if(IsStillborn(same))
            StillbornParticleLogic(x,y,same);
        else
        if ( fastrand() >= FASTRAND_MAX / 13 && same % 2 == 0) MoveParticle(x,y,same); //THe rand condition makes the particles fall unevenly
    }

}

// Updating the particle system (virtual screen) pixel by pixel
inline void UpdateVirtualScreen()
{
    for(int y =0; y< HEIGHT-DASHBOARD_HEIGHT; y++)
    {
        // Due to biasing when iterating through the scanline from left to right,
        // we now chose our direction randomly per scanline.
        if (fastrand() % 2 == 0)
            for(int x = WIDTH-2; x--;) UpdateVirtualPixel(x,y);
        else
            for(int x = 1; x < WIDTH - 1; x++) UpdateVirtualPixel(x,y);
    }
}

//Cearing the particle system
void Clear()
{
    for(int w = 0; w < WIDTH ; w++)
    {
        for(int h = 0; h < HEIGHT; h++)
        {
            vs[w+(WIDTH*h)] = NOTHING;
        }
    }
}

void InitButtons()
{
    // Update dashboard
    SDL_Rect dashboard ;
    dashboard.x = 0;
    dashboard.y = HEIGHT-DASHBOARD_HEIGHT;
    dashboard.w = WIDTH;
    dashboard.h = DASHBOARD_HEIGHT;
    SDL_SetRenderDrawColor( renderer, 155, 155, 155, 255 );

    SDL_RenderFillRect( renderer, &dashboard );
    // ?? SDL_RenderPresent(_renderer);
    //SDL_UpdateRect ( screen , dashboard.x , dashboard.y , dashboard.w , dashboard.h ) ;

    //set up water emit button
    SDL_Rect wateroutput ;
    wateroutput.x = 1;
    wateroutput.y = UPPER_ROW_Y;
    wateroutput.w = BUTTON_SIZE;
    wateroutput.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[WATER].r, colors[WATER].g, colors[WATER].b, colors[WATER].a );
    SDL_RenderFillRect( renderer, &wateroutput );

    ButtonRect warect;
    warect.rect = wateroutput;
    warect.particleType = WATER;
    Button[0] = warect;


    //set up sand sand button
    SDL_Rect sandoutput ;
    sandoutput.x = BUTTON_SIZE + 1;
    sandoutput.y = UPPER_ROW_Y;
    sandoutput.w = BUTTON_SIZE;
    sandoutput.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[SAND].r, colors[SAND].g, colors[SAND].b, colors[SAND].a );
    SDL_RenderFillRect( renderer, &sandoutput );

    ButtonRect sanrect;
    sanrect.rect = sandoutput;
    sanrect.particleType = SAND;
    Button[1] = sanrect;

    //set up salt emit button
    SDL_Rect saltoutput ;
    saltoutput.x = BUTTON_SIZE*2 + 1;
    saltoutput.y = UPPER_ROW_Y;
    saltoutput.w = BUTTON_SIZE;
    saltoutput.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[SALT].r, colors[SALT].g, colors[SALT].b, colors[SALT].a );
    SDL_RenderFillRect( renderer, &saltoutput );

    ButtonRect sarect;
    sarect.rect = saltoutput;
    sarect.particleType = SALT;
    Button[2] = sarect;

    //set up oil button
    SDL_Rect oiloutput ;
    oiloutput.x = BUTTON_SIZE*3 + 1;
    oiloutput.y = UPPER_ROW_Y;
    oiloutput.w = BUTTON_SIZE;
    oiloutput.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[OIL].r, colors[OIL].g, colors[OIL].b, colors[OIL].a );
    SDL_RenderFillRect( renderer, &oiloutput );

    ButtonRect oirect;
    oirect.rect = oiloutput;
    oirect.particleType = OIL;
    Button[3] = oirect;

    //set up fire button
    SDL_Rect fire ;
    fire.x = BUTTON_SIZE*4 + 1;
    fire.y = UPPER_ROW_Y;
    fire.w = BUTTON_SIZE;
    fire.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[FIRE].r, colors[FIRE].g, colors[FIRE].b, colors[FIRE].a );
    SDL_RenderFillRect( renderer, &fire );

    ButtonRect firect;
    firect.particleType = FIRE;
    firect.rect = fire;
    Button[4] = firect;

    //set up acid button
    SDL_Rect acid ;
    acid.x = BUTTON_SIZE*5 + 1;
    acid.y = UPPER_ROW_Y;
    acid.w = BUTTON_SIZE;
    acid.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[ACID].r, colors[ACID].g, colors[ACID].b, colors[ACID].a );
    SDL_RenderFillRect( renderer, &acid );

    ButtonRect acrect;
    acrect.particleType = ACID;
    acrect.rect = acid;
    Button[5] = acrect;

    //set up dirt emit button
    SDL_Rect dirtoutput ;
    dirtoutput.x = BUTTON_SIZE*6 + 1;
    dirtoutput.y = UPPER_ROW_Y;
    dirtoutput.w = BUTTON_SIZE;
    dirtoutput.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[DIRT].r, colors[DIRT].g, colors[DIRT].b, colors[DIRT].a );
    SDL_RenderFillRect( renderer, &dirtoutput );

    ButtonRect direct;
    direct.rect = dirtoutput;
    direct.particleType = DIRT;
    Button[6] = direct;

    //set up spout water
    SDL_Rect spwater ;
    spwater.x = 75 + 1;
    spwater.y = MIDDLE_ROW_Y;
    spwater.w = BUTTON_SIZE;
    spwater.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[WATERSPOUT].r, colors[WATERSPOUT].g, colors[WATERSPOUT].b, colors[WATERSPOUT].a );
    SDL_RenderFillRect( renderer, &spwater );

    ButtonRect spwrect;
    spwrect.particleType = WATERSPOUT;
    spwrect.rect = spwater;
    Button[7] = spwrect;

    //set up spout sand
    SDL_Rect spdirt ;
    spdirt.x = 75 + BUTTON_SIZE + 1;
    spdirt.y = MIDDLE_ROW_Y;
    spdirt.w = BUTTON_SIZE;
    spdirt.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[SANDSPOUT].r, colors[SANDSPOUT].g, colors[SANDSPOUT].b, colors[SANDSPOUT].a );
    SDL_RenderFillRect( renderer, &spdirt );

    ButtonRect spdrect;
    spdrect.particleType = SANDSPOUT;
    spdrect.rect = spdirt;
    Button[8] = spdrect;

    //set up spout salt
    SDL_Rect spsalt ;
    spsalt.x = 75 + BUTTON_SIZE*2 + 1;
    spsalt.y = MIDDLE_ROW_Y;
    spsalt.w = BUTTON_SIZE;
    spsalt.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[SALTSPOUT].r, colors[SALTSPOUT].g, colors[SALTSPOUT].b, colors[SALTSPOUT].a );
    SDL_RenderFillRect( renderer, &spsalt );

    ButtonRect spsrect;
    spsrect.particleType = SALTSPOUT;
    spsrect.rect = spsalt;
    Button[9] = spsrect;

    //set up spout oil
    SDL_Rect spoil ;
    spoil.x = 75 + BUTTON_SIZE*3 + 1;
    spoil.y = MIDDLE_ROW_Y;
    spoil.w = BUTTON_SIZE;
    spoil.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[OILSPOUT].r, colors[OILSPOUT].g, colors[OILSPOUT].b, colors[OILSPOUT].a );
    SDL_RenderFillRect( renderer, &spoil );

    ButtonRect sporect;
    sporect.particleType = OILSPOUT;
    sporect.rect = spoil;
    Button[10] = sporect;

    //set up wall button
    SDL_Rect wall ;
    wall.x = 120 + 1;
    wall.y = LOWER_ROW_Y;
    wall.w = BUTTON_SIZE;
    wall.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[WALL].r, colors[WALL].g, colors[WALL].b, colors[WALL].a );
    SDL_RenderFillRect( renderer, &wall );

    ButtonRect walrect;
    walrect.particleType = WALL;
    walrect.rect = wall;
    Button[11] = walrect;

    //set up torch button
    SDL_Rect torch ;
    torch.x = 120 + BUTTON_SIZE + 1;
    torch.y = LOWER_ROW_Y;
    torch.w = BUTTON_SIZE;
    torch.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[TORCH].r, colors[TORCH].g, colors[TORCH].b, colors[TORCH].a );
    SDL_RenderFillRect( renderer, &torch );

    ButtonRect torect;
    torect.particleType = TORCH;
    torect.rect = torch;
    Button[12] = torect;

    //set up stove button
    SDL_Rect stove ;
    stove.x = 120 + BUTTON_SIZE*2 + 1;
    stove.y = LOWER_ROW_Y;
    stove.w = BUTTON_SIZE;
    stove.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[STOVE].r, colors[STOVE].g, colors[STOVE].b, colors[STOVE].a );
    SDL_RenderFillRect( renderer, &stove );

    ButtonRect storect;
    storect.particleType = STOVE;
    storect.rect = stove;
    Button[13] = storect;

    //set up plant button
    SDL_Rect plant ;
    plant.x = 120 + BUTTON_SIZE*3 + 1;
    plant.y = LOWER_ROW_Y;
    plant.w = BUTTON_SIZE;
    plant.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[PLANT].r, colors[PLANT].g, colors[PLANT].b, colors[PLANT].a );
    SDL_RenderFillRect( renderer, &plant );

    ButtonRect prect;
    prect.particleType = PLANT;
    prect.rect = plant;
    Button[14] = prect;

    //ICE
    SDL_Rect ice ;
    ice.x = 120 + BUTTON_SIZE*4 + 1;
    ice.y = LOWER_ROW_Y;
    ice.w = BUTTON_SIZE;
    ice.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[ICE].r, colors[ICE].g, colors[ICE].b, colors[ICE].a );
    SDL_RenderFillRect( renderer, &ice );

    ButtonRect irect;
    irect.particleType = ICE;
    irect.rect = ice;
    Button[15] = irect;

    //IRONWALL
    SDL_Rect iw ;
    iw.x = 120 + BUTTON_SIZE*5 + 1;
    iw.y = LOWER_ROW_Y;
    iw.w = BUTTON_SIZE;
    iw.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[IRONWALL].r, colors[IRONWALL].g, colors[IRONWALL].b, colors[IRONWALL].a );
    SDL_RenderFillRect( renderer, &iw );

    ButtonRect iwrect;
    iwrect.particleType = IRONWALL;
    iwrect.rect = iw;
    Button[16] = iwrect;

    //VOID
    SDL_Rect voidele ;
    voidele.x = 120 + BUTTON_SIZE*6 + 1;
    voidele.y = LOWER_ROW_Y;
    voidele.w = BUTTON_SIZE;
    voidele.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, colors[VOID].r, colors[VOID].g, colors[VOID].b, colors[VOID].a );
    SDL_RenderFillRect( renderer, &voidele );

    ButtonRect voidelerect;
    voidelerect.particleType = VOID;
    voidelerect.rect = voidele;
    Button[17] = voidelerect;

    //eraser
    SDL_Rect eraser ;
    eraser.x = 195 + 1;
    eraser.y = LOWER_ROW_Y;
    eraser.w = BUTTON_SIZE;
    eraser.h = BUTTON_SIZE;

    SDL_SetRenderDrawColor( renderer, 0, 0, 0, 0 );
    SDL_RenderFillRect( renderer, &eraser );

    ButtonRect eraserrect;
    eraserrect.particleType = NOTHING;
    eraserrect.rect = eraser;
    Button[18] = eraserrect;
}

// Initializing the screen
void init()
{
    SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);

    // Initializing SDL
    if ( SDL_Init(SDL_INIT_EVERYTHING) < 0 )
    {
        fprintf( stderr, "Video initialization failed: %s\n",
                 SDL_GetError( ) );
        SDL_Quit( );
    }

    if(SDL_NumJoysticks() > 0) {
        SDL_JoystickOpen(0);
        SDL_GameControllerOpen(0);
    }

    SDL_ShowCursor(0);

    // Nearest-neighbor scaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    /* VITA: Disable Back Touchpad to prevent "misclicks" */


    //Creating the screen using 16-bit colors
    window = SDL_CreateWindow("SDL2Sand",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              WIDTH, HEIGHT,
                              SDL_WINDOW_FULLSCREEN_DESKTOP);
    if ( window == nullptr ) {
        fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);
    screen_buffer = (uint32_t *)calloc(WIDTH * HEIGHT, sizeof(uint32_t));
    if (!screen_buffer) {
        fprintf(stderr, "Failed to allocate screen buffer");
    }

    initColors();

    scene.x = 0;
    scene.y = 0;
    scene.w = WIDTH;
    scene.h = HEIGHT-DASHBOARD_HEIGHT;

    scene_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, scene.w, scene.h);

    InitButtons();

    SDL_RenderPresent(renderer);
}


inline void CheckGuiInteraction()
{
    for(int i = BUTTON_COUNT; i--;)
    {
        ButtonRect r = Button[i];
        if(mbx > r.rect.x && mbx <= r.rect.x+r.rect.w && mby > r.rect.y && mby <= r.rect.y+r.rect.h)
            CurrentParticleType = r.particleType;
    }
}

void drawSelection()
{
    for(int i = BUTTON_COUNT; i--;)
    {
        ButtonRect r = Button[i];

        SDL_Rect top = {r.rect.x, r.rect.y, r.rect.w + 1, 1};
        SDL_Rect bottom = {r.rect.x, r.rect.y + r.rect.h, r.rect.w + 1, 1};
        SDL_Rect left = {r.rect.x, r.rect.y, 1, r.rect.h + 1};
        SDL_Rect right = {r.rect.x + r.rect.w, r.rect.y, 1, r.rect.h + 1};

        if (CurrentParticleType == r.particleType)
        {
            SDL_SetRenderDrawColor( renderer, 255, 0, 255, 255 );
            SDL_RenderFillRect( renderer, &top );
            SDL_RenderFillRect( renderer, &bottom );
            SDL_RenderFillRect( renderer, &left );
            SDL_RenderFillRect( renderer, &right );
        }
        else
        {
            SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
            SDL_RenderFillRect( renderer, &top );
            SDL_RenderFillRect( renderer, &bottom );
            SDL_RenderFillRect( renderer, &left );
            SDL_RenderFillRect( renderer, &right );
        }
    }
}

void drawCursor(int x, int y)
{
    SDL_Rect partHorizontal = { x+1, y+1, 4, 1 };
    SDL_Rect partVertical = { x+1, y+1, 1, 4 };

    SDL_SetRenderDrawColor( renderer, 255, 0, 255, 255 );
    SDL_RenderFillRect( renderer, &partHorizontal );
    SDL_RenderFillRect( renderer, &partVertical );
}

void drawPenSize()
{
    SDL_Rect size = { WIDTH - BUTTON_SIZE, HEIGHT - BUTTON_SIZE - 1, 0, 0 };

    switch(penSize)
    {
        case 1:
            size.w = 1;
            size.h = 1;
            break;
        case 2:
            size.w = 2;
            size.h = 2;
            break;
        case 4:
            size.w = 3;
            size.h = 3;
            break;
        case 8:
            size.w = 5;
            size.h = 5;
            break;
        case 16:
            size.w = 7;
            size.h = 7;
            break;
        case 32:
            size.w = 9;
            size.h = 9;
            break;

        default:
            break;
    }

    SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
    SDL_RenderFillRect( renderer, &size );
}


int main(int argc, char **argv)
{

    CCmdLine cmdLine;

    // parse the command line.
    if (cmdLine.SplitLine(argc, argv) < 1)
    {
        // no switches were given on the command line
        //Set default size
        HEIGHT = 170;
        WIDTH = 300;
    }
    else
    {
        // StringType is defined in CmdLine.h.
        // it is CString when using MFC, else STL's 'string'
        StringType height, width;

        // get the required arguments
        try
        {
            // if any of these GetArgument calls fail,
            // we'll end up in the catch() block

            height = cmdLine.GetArgument("-height", 0);

            width = cmdLine.GetArgument("-width", 0);
            HEIGHT = atoi(height.c_str());
            WIDTH = atoi(width.c_str());
        }
        catch (...)
        {
            // one of the required arguments was missing, abort
            exit(-1);
        }
    }

    UPPER_ROW_Y = HEIGHT - BUTTON_SIZE - 1;
    MIDDLE_ROW_Y = HEIGHT - BUTTON_SIZE - 1;
    LOWER_ROW_Y = HEIGHT - BUTTON_SIZE - 1;

    vs = new ParticleType[HEIGHT*WIDTH];


    init();
    Clear();

    int done=0;

    //To emit or not to emit
    bool emitWater = true;
    bool emitSand = true;
    bool emitSalt = true;
    bool emitOil = true;

    //Initial density of emitters
    float waterDens = 0.3f;
    float sandDens = 0.3f;
    float saltDens = 0.3f;
    float oilDens = 0.3f;

    // Set initial seed
    fast_srand( (unsigned)time( nullptr ) );

    int oldx = WIDTH/2, oldy = HEIGHT/2;

    //Mouse button pressed down?
    bool down = false;

    //Used for calculating the average FPS from NumFrames
    const int NumFrames = 20;
    int AvgFrameTimes[NumFrames];
    for(int & AvgFrameTime : AvgFrameTimes)
        AvgFrameTime = 0;
    int FrameTime = 0;
    int PrevFrameTime = 0;
    int Index = 0;

    int slow = false;

    //The frames per second cap timer
    LTimer capTimer;

    //The game loop
    while(done == 0)
    {
        //Start cap timer
        capTimer.start();

        SDL_Event event;
        //Polling events
        while ( SDL_PollEvent(&event) )
        {
            if ( event.type == SDL_QUIT )  {  done = 1;  }
            //Key strokes
            if ( event.type == SDL_CONTROLLERBUTTONDOWN )
            {
                switch (event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_START:
                    case SDL_CONTROLLER_BUTTON_BACK:
                        Clear();
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        for(int i = BUTTON_COUNT; i--;)
                        {
                            if(CurrentParticleType == Button[i].particleType)
                            {
                                if(i > 0)
                                {
                                    CurrentParticleType = Button[i-1].particleType;
                                }
                                else
                                {
                                    CurrentParticleType = Button[BUTTON_COUNT-1].particleType;
                                }

                                break;
                            }
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        for(int i = BUTTON_COUNT; i--;)
                        {
                            if(CurrentParticleType == Button[i].particleType)
                            {
                                if(i < BUTTON_COUNT - 1)
                                {
                                    CurrentParticleType = Button[i+1].particleType;
                                }
                                else
                                {
                                    CurrentParticleType = Button[0].particleType;
                                }

                                break;
                            }
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: // Increase pen size
                        penSize *= 2;
                        if (penSize > 32)
                            penSize = 32;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: // Decrease pen size
                        penSize /= 2;
                        if(penSize < 1)
                            penSize = 1;
                        break;
                    case SDL_CONTROLLER_BUTTON_A:	// A
                        mbx = oldx;
                        mby = oldy;

                        if(oldy < (HEIGHT-DASHBOARD_HEIGHT))
                            DrawLine(oldx+speedX,oldy+speedY,oldx,oldy);

                        down = true;
                        break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        slow ^= true;
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                        emitOil ^= true;
                        emitSalt ^= true;
                        emitWater ^= true;
                        emitSand ^= true;
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        LastParticleType = CurrentParticleType;
                        CurrentParticleType = NOTHING;
                        down = true;
                        break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        oilDens -= 0.05f;
                        if(oilDens < 0.05f)
                            oilDens = 0.05f;
                        saltDens -= 0.05f;
                        if(saltDens < 0.05f)
                            saltDens = 0.05f;
                        waterDens -= 0.05f;
                        if(waterDens < 0.05f)
                            waterDens = 0.05f;
                        sandDens -= 0.05f;
                        if(sandDens < 0.05f)
                            sandDens = 0.05f;
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        oilDens += 0.05f;
                        if(oilDens > 1.0f)
                            oilDens = 1.0f;
                        saltDens += 0.05f;
                        if(saltDens > 1.0f)
                            saltDens = 1.0f;
                        waterDens += 0.05f;
                        if(waterDens > 1.0f)
                            waterDens = 1.0f;
                        sandDens += 0.05f;
                        if(sandDens > 1.0f)
                            sandDens = 1.0f;
                        break;

                    default:
                        break;
                }
            }
            if ( event.type == SDL_CONTROLLERBUTTONUP )
            {
                switch (event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_A:
                        mbx = 0;
                        mby = 0;
                        down = false;
                        break;

                    case SDL_CONTROLLER_BUTTON_B:
                        CurrentParticleType = LastParticleType;
                        mbx = 0;
                        mby = 0;
                        down = false;
                        break;

                    default:
                        break;
                }
            }
            // Analog joystick movement
            if(event.type == SDL_JOYAXISMOTION)
            {
                switch(event.jaxis.axis)
                {
                    case 0:		// axis 0 (left-right)
                        if(event.jaxis.value < -JOY_DEADZONE && canMoveX)
                        {
                            // left movement
                            speedX = (6 * event.jaxis.value / 65535);
                        }
                        else if(event.jaxis.value > JOY_DEADZONE && canMoveX)
                        {
                            // right movement
                            speedX = (6 * event.jaxis.value / 65535);
                        }
                        else if(event.jaxis.value > -JOY_DEADZONE && event.jaxis.value < JOY_DEADZONE)
                        {
                            speedX = 0;
                            canMoveX = 1;
                        }
                        break;
                    case 1:		// axis 1 (up-down)
                        if(event.jaxis.value < -JOY_DEADZONE && canMoveY)
                        {
                            // up movement
                            speedY = (6 * event.jaxis.value / 65535);
                        }
                        else if(event.jaxis.value > JOY_DEADZONE && canMoveY)
                        {
                            // down movement
                            speedY = (6 * event.jaxis.value / 65535);
                        }
                        else if(event.jaxis.value > -JOY_DEADZONE && event.jaxis.value < JOY_DEADZONE)
                        {
                            speedY = 0;
                            canMoveY = 1;
                        }
                        break;

                    default:
                        break;
                }
            }
            // If mouse button pressed then save position of cursor
            if( event.type == SDL_MOUSEBUTTONDOWN)
            {
                SDL_MouseButtonEvent mbe = (SDL_MouseButtonEvent) event.button;
                oldx = mbe.x; oldy=mbe.y;
                mbx = mbe.x;
                mby = mbe.y;
                if(mbe.x < (HEIGHT-DASHBOARD_HEIGHT))
                    DrawLine(mbe.x,mbe.y,oldx,oldy);
                down = true;
            }
            // Button released
            if(event.type == SDL_MOUSEBUTTONUP)
            {
                SDL_MouseButtonEvent mbe = (SDL_MouseButtonEvent) event.button;
                if(oldy < (HEIGHT-DASHBOARD_HEIGHT))
                    DrawLine(mbe.x,mbe.y,oldx,oldy);
                mbx = 0;
                mby = 0;
                down = false;

            }
            // Mouse has moved
            if(event.type == SDL_MOUSEMOTION)
            {
                SDL_MouseMotionEvent mme = (SDL_MouseMotionEvent) event.motion;
                if(mme.state & SDL_BUTTON(1))
                    DrawLine(mme.x,mme.y,oldx,oldy);
                oldx = mme.x; oldy=mme.y;
            }
            if(mby > HEIGHT-DASHBOARD_HEIGHT)
                CheckGuiInteraction();
        }

        if(slow)
        {
            if(speedX > 0)
                oldx += 1;
            else if(speedX < 0)
                oldx += -1;
            if(speedY > 0)
                oldy += 1;
            else if(speedY < 0)
                oldy += -1;
        }
        else
        {
            oldx += speedX;
            oldy += speedY;
        }

        if(oldx < 0)
            oldx = 0;
        else if(oldx > WIDTH)
            oldx = WIDTH;

        if(oldy < 0)
            oldy = 0;
        else if(oldy > HEIGHT)
            oldy = HEIGHT;

        //To emit or not to emit
        if(emitWater)
            Emit((WIDTH/2 - ((WIDTH/6)*2)), 20, WATER, waterDens);
        if(emitSand)
            Emit((WIDTH/2 - (WIDTH/6)), 20, SAND, sandDens);
        if(emitSalt)
            Emit((WIDTH/2 + (WIDTH/6)), 20, SALT, saltDens);
        if(emitOil)
            Emit((WIDTH/2 + ((WIDTH/6)*2)), 20, OIL, oilDens);

        //If the button is pressed (and no event has occured since last frame due
        // to the polling procedure, then draw at the position (enabeling 'dynamic emitters')
        if(down)
            DrawLine(oldx,oldy,oldx,oldy);

        //Clear bottom line
        for (int i=0; i< WIDTH; i++) vs[i+((HEIGHT-DASHBOARD_HEIGHT-1)*WIDTH)] = NOTHING;
        //Clear top line
        for (int i=0; i< WIDTH; i++) vs[i+((0)*WIDTH)] = NOTHING;

        // Update the virtual screen (performing particle logic)
        UpdateVirtualScreen();

        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);
        // Map the virtual screen to the real screen
        DrawScene();
        InitButtons();
        drawSelection();
        drawPenSize();
        drawCursor(oldx, oldy);
        //Fip the vs
        SDL_RenderPresent(renderer);

        //If frame finished early
        int frameTicks = capTimer.getTicks();
        if( frameTicks < SCREEN_TICKS_PER_FRAME )
        {
            //Wait remaining time
            SDL_Delay( SCREEN_TICKS_PER_FRAME - frameTicks );
        }
    }

    //Loop ended - quit SDL
    SDL_Quit( );
    if(SDL_NumJoysticks() > 0)
        SDL_JoystickClose(nullptr);
    return 0;
}

#pragma clang diagnostic pop
#pragma clang diagnostic pop