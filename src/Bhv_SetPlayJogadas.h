// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Tanilson Dias dos Santos
 
 Universidade Federal do Tocantins - Maio/2010

*/
/////////////////////////////////////////////////////////////////////

#ifndef BHV_SET_PLAY_JOGADAS_H
#define BHV_SET_PLAY_JOGADAS_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>

// our free kick

class Bhv_SetPlayJogadas
    : public rcsc::SoccerBehavior {
public:

      Bhv_SetPlayJogadas()
      { };

  bool execute( rcsc::PlayerAgent * agent );

private:

    void doKick( rcsc::PlayerAgent * agent );
    bool doKickWait( rcsc::PlayerAgent * agent );

    void doMove( rcsc::PlayerAgent * agent );
};

#endif
