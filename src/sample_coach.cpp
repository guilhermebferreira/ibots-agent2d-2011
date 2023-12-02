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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sample_coach.h"

#include <rcsc/coach/coach_command.h>
#include <rcsc/coach/coach_config.h>
#include <rcsc/common/basic_client.h>
#include <rcsc/common/logger.h>
#include <rcsc/common/player_param.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/player_type.h>
#include <rcsc/common/audio_memory.h>
#include <rcsc/common/say_message_parser.h>
#include <rcsc/param/param_map.h>
#include <rcsc/param/cmd_line_parser.h>

#include <cstdio>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <functional>

#include "team_logo.xpm"

using namespace rcsc;

/////////////////////////////////////////////////////////

struct TypeStaminaIncComp
    : public std::binary_function< const PlayerType *,
                                   const PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          return lhs->staminaIncMax() < rhs->staminaIncMax();
      }

};

/////////////////////////////////////////////////////////

struct RealSpeedMaxCmp
    : public std::binary_function< const PlayerType *,
                                   const PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          if ( std::fabs( lhs->realSpeedMax() - rhs->realSpeedMax() ) < 0.005 )
          {
              return lhs->cyclesToReachMaxSpeed() < rhs->cyclesToReachMaxSpeed();
          }

          return lhs->realSpeedMax() > rhs->realSpeedMax();
      }

};

/////////////////////////////////////////////////////////

struct MaxSpeedReachStepCmp
    : public std::binary_function< const PlayerType *,
                                   const PlayerType *,
                                   bool > {

    result_type operator()( first_argument_type lhs,
                            second_argument_type rhs ) const
      {
          return lhs->cyclesToReachMaxSpeed() < rhs->cyclesToReachMaxSpeed();
      }

};

/*-------------------------------------------------------------------*/
/*!

*/
SampleCoach::SampleCoach()
    : CoachAgent()
    , M_substitute_count( 0 )
{
    //
    // register audio memory & say message parsers
    //

    boost::shared_ptr< AudioMemory > audio_memory( new AudioMemory );

    M_worldmodel.setAudioMemory( audio_memory );

    addSayMessageParser( SayMessageParser::Ptr( new BallMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new PassMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new InterceptMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new GoalieMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new GoalieAndPlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new OffsideLineMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new DefenseLineMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new WaitRequestMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new PassRequestMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new DribbleMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new BallGoalieMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new OnePlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new TwoPlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new ThreePlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new SelfMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new TeammateMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new OpponentMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new BallPlayerMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new StaminaMessageParser( audio_memory ) ) );
    addSayMessageParser( SayMessageParser::Ptr( new RecoveryMessageParser( audio_memory ) ) );

    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 9 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 8 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 7 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 6 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 5 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 4 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 3 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 2 >( audio_memory ) ) );
    // addSayMessageParser( SayMessageParser::Ptr( new FreeMessageParser< 1 >( audio_memory ) ) );

    //
    //
    //

    for ( int i = 0; i < 11; ++i )
    {
        // init map values
        M_assigned_player_type_id[i] = Hetero_Default;
    }

    for ( int i = 0; i < 11; ++i )
    {
        M_opponent_player_types[i] = Hetero_Default;
    }

    for ( int i = 0; i < 11; ++i )
    {
        M_teammate_recovery[i] = ServerParam::i().recoverInit();
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
SampleCoach::~SampleCoach()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
bool
SampleCoach::initImpl( CmdLineParser & cmd_parser )
{
    bool result =CoachAgent::initImpl( cmd_parser );

#if 0
    ParamMap my_params;
    if ( cmd_parser.count( "help" ) )
    {
       my_params.printHelp( std::cout );
       return false;
    }
    cmd_parser.parse( my_params );
#endif

    if ( cmd_parser.failed() )
    {
        std::cerr << "coach: ***WARNING*** detected unsupported options: ";
        cmd_parser.print( std::cerr );
        std::cerr << std::endl;
    }

    if ( ! result )
    {
        return false;
    }

    //////////////////////////////////////////////////////////////////
    // Add your code here.
    //////////////////////////////////////////////////////////////////

    if ( config().useTeamGraphic() )
    {
        if ( config().teamGraphicFile().empty() )
        {
            M_team_graphic.createXpmTiles( team_logo_xpm );
        }
        else
        {
            M_team_graphic.readXpmFile( config().teamGraphicFile().c_str() );
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::actionImpl()
{
    if ( world().time().cycle() == 0
         && config().useTeamGraphic()
         && M_team_graphic.tiles().size() != teamGraphicOKSet().size() )
    {
        sendTeamGraphic();
    }

    updateRecoveryInfo();

    doSubstitute();
    sayPlayerTypes();
//     if ( world().time().cycle() > 0 )
//     {
//         M_client->setServerAlive( false );
//     }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::updateRecoveryInfo()
{
    const int half_time = ServerParam::i().halfTime() * 10;
    const int normal_time = half_time * ServerParam::i().nrNormalHalfs();

    if ( world().time().cycle() < normal_time
         && world().gameMode().type() == GameMode::BeforeKickOff )
    {
        for ( int i = 0; i < 11; ++i )
        {
            M_teammate_recovery[i] = ServerParam::i().recoverInit();
        }

        return;
    }

    if ( world().audioMemory().recoveryTime() != world().time() )
    {
        return;
    }

#if 0
    std::cerr << world().time() << ": heared recovery:\n";
    for ( std::vector< AudioMemory::Recovery >::const_iterator it = world().audioMemory().recovery().begin();
          it != world().audioMemory().recovery().end();
          ++it )
    {
        double recovery
            = it->rate_ * ( ServerParam::i().recoverInit() - ServerParam::i().recoverMin() )
            + ServerParam::i().recoverMin();

            std::cerr << "  sender=" << it->sender_
                      << " rate=" << it->rate_
                      << " value=" << recovery
                      << '\n';

    }
    std::cerr << std::flush;
#endif
    const std::vector< AudioMemory::Recovery >::const_iterator end = world().audioMemory().recovery().end();
    for ( std::vector< AudioMemory::Recovery >::const_iterator it = world().audioMemory().recovery().begin();
          it != end;
          ++it )
    {
        if ( 1 <= it->sender_
             && it->sender_ <= 11 )
        {
            double value
                = it->rate_ * ( ServerParam::i().recoverInit() - ServerParam::i().recoverMin() )
                + ServerParam::i().recoverMin();
            // std::cerr << "coach: " << world().time() << " heared recovery: sender=" << it->sender_
            //           << " value=" << value << std::endl;
            M_teammate_recovery[ it->sender_ - 1 ] = value;
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::doSubstitute()
{
    static bool S_first_substituted = false;

    if ( ! S_first_substituted
         && world().time().cycle() == 0
         && world().time().stopped() > 10 )
    {
        doFirstSubstitute();
        S_first_substituted = true;

        return;
    }

    if ( world().time().cycle() > 0
         && world().gameMode().type() != GameMode::PlayOn
         && ! world().gameMode().isPenaltyKickMode() )
    {
        doSubstituteTiredPlayers();

        return;
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::doFirstSubstitute()
{
    PlayerTypePtrCont candidates;

    std::fprintf( stderr,
                  "id speed step inc  power  stam"
                  //" decay"
                  //" moment"
                  //" dprate"
                  "  karea"
                  //"  krand"
                  //" effmax effmin"
                  "\n" );

    for ( PlayerTypeSet::PlayerTypeMap::const_iterator it = PlayerTypeSet::i().playerTypeMap().begin();
          it != PlayerTypeSet::i().playerTypeMap().end();
          ++it )
    {
        if ( it->second.id() == Hetero_Default )
        {
            if ( PlayerParam::i().allowMultDefaultType() )
            {
                for ( int i = 0; i <= MAX_PLAYER; ++i )
                {
                    M_available_player_type_id.push_back( Hetero_Default );
                    candidates.push_back( &(it->second) );
                }
            }
            else
            {
#ifdef USE_HETERO_GOALIE
                const int pt_max = ( config().version() < 14.0
                                     ? PlayerParam::i().ptMax() - 1
                                     : PlayerParam::i().ptMax() );
                for ( int i = 0; i < pt_max; ++i )
                {
                    M_available_player_type_id.push_back( it->second.id() );
                    candidates.push_back( &(it->second) );
                }
#endif
            }
        }
        else
        {
            for ( int i = 0; i < PlayerParam::i().ptMax(); ++i )
            {
                M_available_player_type_id.push_back( it->second.id() );
                candidates.push_back( &(it->second) );
            }
        }
        std::fprintf( stderr,
                      " %d %.3f  %2d  %.1f %5.1f %5.1f"
                      //" %.3f"
                      //"  %4.1f"
                      //"  %.5f"
                      "  %.3f"
                      //"  %.2f"
                      //"  %.3f  %.3f"
                      "\n",
                      it->second.id(),
                      it->second.realSpeedMax(),
                      it->second.cyclesToReachMaxSpeed(),
                      it->second.staminaIncMax(),
                      it->second.getDashPowerToKeepMaxSpeed(),
                      it->second.getOneStepStaminaComsumption(),
                      //it->second.playerDecay(),
                      //it->second.inertiaMoment(),
                      //it->second.dashPowerRate(),
                      it->second.kickableArea()
                      //it->second.kickRand(),
                      //it->second.effortMax(), it->second.effortMin()
                      );
    }


//     std::cerr << "total available types = ";
//     for ( PlayerTypePtrCont::iterator it = candidates.begin();
//           it != candidates.end();
//           ++it )
//     {
//         std::cerr << (*it)->id() << ' ';
//     }
//     std::cerr << std::endl;

    std::vector< int > unum_order;
    unum_order.reserve( 11 );

#ifdef USE_HETERO_GOALIE
    if ( config().version() >= 14.0 )
    {
        unum_order.push_back( 1 ); // goalie
    }
#endif

#if 0
    // side back has priority
    unum_order.push_back( 11 );
    unum_order.push_back( 2 );
    unum_order.push_back( 3 );
    unum_order.push_back( 4 );
    unum_order.push_back( 5 );
    unum_order.push_back( 10 );
    unum_order.push_back( 9 );
    unum_order.push_back( 6 );
    unum_order.push_back( 7 );
    unum_order.push_back( 8 );
#else
    // wing player has priority
    unum_order.push_back( 11 );
    unum_order.push_back( 2 );
    unum_order.push_back( 3 );
    unum_order.push_back( 10 );
    unum_order.push_back( 9 );
    unum_order.push_back( 6 );
    unum_order.push_back( 4 );
    unum_order.push_back( 5 );
    unum_order.push_back( 7 );
    unum_order.push_back( 8 );
#endif

    for ( std::vector< int >::iterator unum = unum_order.begin();
          unum != unum_order.end();
          ++unum )
    {
        int type = getFastestType( candidates );
        if ( type != Hetero_Unknown )
        {
            substituteTo( *unum, type );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::doSubstituteTiredPlayers()
{
    if ( M_substitute_count >= PlayerParam::i().subsMax() )
    {
        // over the maximum substitution
        return;
    }

    const ServerParam & SP = ServerParam::i();

    //
    // check game time
    //
    const int half_time = SP.halfTime() * 10;
    const int normal_time = half_time * SP.nrNormalHalfs();

    if ( world().time().cycle() < normal_time - 500
         || world().time().cycle() <= half_time + 1
         || world().gameMode().type() == GameMode::KickOff_ )
    {
        return;
    }

    std::cerr << "coach: try substitute. " << world().time() << std::endl;

    //
    // create candidate teamamte
    //
    std::vector< int > tired_teammate_unum;

    for ( int i = 0; i < 11; ++i )
    {
        if ( M_teammate_recovery[i] < ServerParam::i().recoverInit() - 0.002 )
        {
            tired_teammate_unum.push_back( i + 1 );
        }
    }

    if ( tired_teammate_unum.empty() )
    {
        std::cerr << "coach: try substitute. " << world().time()
                  << " no tired teammate."
                  << std::endl;
        return;
    }

    //
    // create candidate player type
    //
    PlayerTypePtrCont candidates;

    for ( std::vector< int >::const_iterator id = M_available_player_type_id.begin();
          id !=  M_available_player_type_id.end();
          ++id )
    {
        const PlayerType * ptype = PlayerTypeSet::i().get( *id );
        if ( ! ptype )
        {
            std::cerr << config().teamName() << " coach "
                      << world().time()
                      << " : Could not get player type. id=" << *id << std::endl;
            continue;
        }

        candidates.push_back( ptype );
    }

    //
    // try substitution
    //
    for ( std::vector< int >::iterator unum = tired_teammate_unum.begin();
          unum != tired_teammate_unum.end();
          ++unum )
    {
        if ( M_substitute_count >= PlayerParam::i().subsMax() )
        {
            // over the maximum substitution
            return;
        }

        int type = getFastestType( candidates );
        if ( type != Hetero_Unknown )
        {
            substituteTo( *unum, type );
        }
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
SampleCoach::substituteTo( const int unum,
                           const int type )
{
    if ( world().time().cycle() > 0
         && M_substitute_count >= PlayerParam::i().subsMax() )
    {
        std::cerr << "***Warning*** "
                  << config().teamName() << " coach: over the substitution max."
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    std::vector< int >::iterator it = std::find( M_available_player_type_id.begin(),
                                                 M_available_player_type_id.end(),
                                                 type );
    if ( it == M_available_player_type_id.end() )
    {
        std::cerr << "***ERROR*** "
                  << config().teamName() << " coach: "
                  << " cannot change the player " << unum
                  << " to type " << type
                  << std::endl;
        return;
    }

    M_available_player_type_id.erase( it );
    M_assigned_player_type_id.insert( std::make_pair( unum, type ) );
    if ( world().time().cycle() > 0 )
    {
        ++M_substitute_count;
    }

    doChangePlayerType( unum, type );

    if ( 1 <= unum && unum <= 11 )
    {
        M_teammate_recovery[unum-1] = ServerParam::i().recoverInit();
    }

    std::cout << config().teamName() << " coach: "
              << "change player " << unum
              << " to type " << type
              << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

 */
int
SampleCoach::getFastestType( PlayerTypePtrCont & candidates )
{
    if ( candidates.empty() )
    {
        return Hetero_Unknown;
    }

    // sort by max speed
    std::sort( candidates.begin(),
               candidates.end(),
               RealSpeedMaxCmp() );

//     std::cerr << "getFastestType candidate = ";
//     for ( PlayerTypePtrCont::iterator it = candidates.begin();
//           it != candidates.end();
//           ++it )
//     {
//         std::cerr << (*it)->id() << ' ';
//     }
//     std::cerr << std::endl;

    PlayerTypePtrCont::iterator best_type = candidates.end();
    double max_speed = 0.0;
    int min_cycle = 100;
    for ( PlayerTypePtrCont::iterator it = candidates.begin();
          it != candidates.end();
          ++it )
    {
        if ( (*it)->realSpeedMax() < max_speed - 0.01 )
        {
            break;
        }

        if ( (*it)->cyclesToReachMaxSpeed() < min_cycle )
        {
            best_type = it;
            max_speed = (*best_type)->realSpeedMax();
            min_cycle = (*best_type)->cyclesToReachMaxSpeed();
            continue;
        }

        if ( (*it)->cyclesToReachMaxSpeed() == min_cycle )
        {
            if ( (*it)->getOneStepStaminaComsumption()
                 < (*best_type)->getOneStepStaminaComsumption() )
            {
                best_type = it;
                max_speed = (*best_type)->realSpeedMax();
            }
        }
    }

    if ( best_type == candidates.end() )
    {
        return Hetero_Unknown;
    }

    int id = (*best_type)->id();
    candidates.erase( best_type );
    return id;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::sayPlayerTypes()
{
    /*
      format:
      "(player_types (1 0) (2 1) (3 2) (4 3) (5 4) (6 5) (7 6) (8 -1) (9 0) (10 1) (11 2))"
      ->
      (say (freeform "(player_type ...)"))
    */

    static GameTime s_last_send_time( 0, 0 );

    if ( ! config().useFreeform() )
    {
        return;
    }

    if ( ! world().canSendFreeform() )
    {
        return;
    }

    int analyzed_count = 0;

    for ( int unum = 1; unum <= 11; ++unum )
    {
        const int id = world().heteroID( world().theirSide(), unum );

        if ( id != M_opponent_player_types[unum - 1] )
        {
            M_opponent_player_types[unum - 1] = id;

            if ( id != Hetero_Unknown )
            {
                ++analyzed_count;
            }
        }
    }

    if ( analyzed_count == 0 )
    {
        return;
    }

    std::string msg;
    msg.reserve( 128 );

    msg = "(player_types ";

    for ( int unum = 1; unum <= 11; ++unum )
    {
        char buf[8];
        snprintf( buf, 8, "(%d %d)",
                  unum, M_opponent_player_types[unum - 1] );
        msg += buf;
    }

    msg += ")";

    doSayFreeform( msg );

    s_last_send_time = world().time();

    std::cout << config().teamName()
              << " Coach: "
              << world().time()
              << " send freeform " << msg
              << std::endl;
}

/*-------------------------------------------------------------------*/
/*!

*/
void
SampleCoach::sendTeamGraphic()
{
    int count = 0;
    for ( TeamGraphic::Map::const_reverse_iterator tile = M_team_graphic.tiles().rbegin();
          tile != M_team_graphic.tiles().rend();
          ++tile )
    {
        if ( teamGraphicOKSet().find( tile->first ) == teamGraphicOKSet().end() )
        {
            if ( ! doTeamGraphic( tile->first.first,
                                  tile->first.second,
                                  M_team_graphic ) )
            {
                break;
            }
            ++count;
        }
    }

    if ( count > 0 )
    {
        std::cout << config().teamName()
                  << " coach: "
                  << world().time()
                  << " send team_graphic " << count << " tiles"
                  << std::endl;
    }
}
