// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hidehisa AKIYAMA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

/////////////////////////////////////////////////////////////////////

#ifndef SAMPLE_COACH_H
#define SAMPLE_COACH_H

#include <vector>
#include <map>
#include <set>

#include <rcsc/types.h>

#include <rcsc/coach/coach_agent.h>

namespace rcsc {
class PlayerType;
}

class SampleCoach
    : public rcsc::CoachAgent {
private:
    typedef std::vector< const rcsc::PlayerType * > PlayerTypePtrCont;

    int M_substitute_count; // substitution count after kick-off
    std::vector< int > M_available_player_type_id;
    std::map< int, int > M_assigned_player_type_id;

    int M_opponent_player_types[11];

    double M_teammate_recovery[11];

    rcsc::TeamGraphic M_team_graphic;

public:

    SampleCoach();

    virtual
    ~SampleCoach();

private:

    void updateRecoveryInfo();

    /*!
      \brief substitute player.

      This methos should be overrided in the derived class
    */
    void doSubstitute();

    void doFirstSubstitute();
    void doSubstituteTiredPlayers();

    void substituteTo( const int unum,
                       const int type );

    int getFastestType( PlayerTypePtrCont & candidates );

    void sayPlayerTypes();

    void sendTeamGraphic();

protected:

    /*!
      You can override this method.
      But you must call PlayerAgent::doInit() in this method.
    */
    virtual
    bool initImpl( rcsc::CmdLineParser & cmd_parser );

    //! main decision
    virtual
    void actionImpl();

};

#endif
