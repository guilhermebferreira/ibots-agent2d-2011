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
#include <config.h>
#endif

#include "bhv_set_play_free_kick.h"

#include "strategy.h"

#include "bhv_set_play.h"
#include "bhv_prepare_set_play_kick.h"
#include "bhv_go_to_static_ball.h"
#include "bhv_chain_action.h"

#include "intention_wait_after_set_play_kick.h"

#include <rcsc/action/basic_actions.h>
#include <rcsc/action/body_go_to_point.h>
#include <rcsc/action/body_kick_one_step.h>
#include <rcsc/action/body_clear_ball.h>
#include <rcsc/action/body_pass.h>
#include <rcsc/action/neck_scan_field.h>
#include <rcsc/action/neck_turn_to_ball_or_scan.h>

#include <rcsc/player/player_agent.h>
#include <rcsc/player/debug_client.h>
#include <rcsc/player/say_message_builder.h>

#include <rcsc/common/logger.h>
#include <rcsc/common/server_param.h>
#include <rcsc/geom/circle_2d.h>
#include <rcsc/math_util.h>
#include <../../librcsc-4.0.0/rcsc/player/say_message_builder.h>
#include </usr/local/include/rcsc/player/enumeracao.h>
#include <rcsc/common/audio_memory.h>

#define VALOR_X 5.0 //CONSTANTE QUE IRÁ DEFINIR DISTÂNCIA EM QUE O JOGADOR VAI SE POSICIONAR COM RELAÇÃO A BOLA NO EIXO X
#define VALOR_Y 2.0 // (...) EM REAÇÃO AO EIXO "Y"
#define PI 3.14159265

using namespace rcsc;

/*-------------------------------------------------------------------*/
/*!
  execute action
*/
bool
Bhv_SetPlayFreeKick::execute( PlayerAgent * agent )
{ 
    dlog.addText( Logger::TEAM,
                  __FILE__": Bhv_SetPlayFreeKick" );
    //---------------------------------------------------
    if ( Bhv_SetPlay::is_kicker( agent ) )
    {
        doKick( agent );
    }
    else
    {
        doMove( agent );
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayFreeKick::doKick( PlayerAgent * agent )
{
    dlog.addText( Logger::TEAM,
                  __FILE__": (doKick)" );
    //
    // go to the ball position
    //
    if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    {
        return;
    }

    //
    // wait
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // kick
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    //
    // pass
    //
    if ( Bhv_ChainAction().execute( agent ) )
    {
        agent->debugClient().addMessage( "FreeKick:Chain" );
        agent->setIntention( new IntentionWaitAfterSetPlayKick() );
        return;
    }
    // {
    //     Vector2D target_point;
    //     double ball_speed = 0.0;
    //     if ( Body_Pass::get_best_pass( wm,
    //                                    &target_point,
    //                                    &ball_speed,
    //                                    NULL )
    //          && ( target_point.x > -25.0
    //               || target_point.x > wm.ball().pos().x + 10.0 )
    //          && ball_speed < max_ball_speed * 1.1 )
    //     {
    //         ball_speed = std::min( ball_speed, max_ball_speed );
    //         agent->debugClient().addMessage( "FreeKick:Pass%.3f", ball_speed );
    //         agent->debugClient().setTarget( target_point );
    //         dlog.addText( Logger::TEAM,
    //                       __FILE__":  pass target=(%.1f %.1f) speed=%.2f",
    //                       target_point.x, target_point.y,
    //                       ball_speed );
    //         Body_KickOneStep( target_point, ball_speed ).execute( agent );
    //         agent->setNeckAction( new Neck_ScanField() );
    //         return;
    //     }
    // }

    //
    // kick to the nearest teammate
    //
    {
        const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if ( nearest_teammate
             && nearest_teammate->distFromSelf() < 20.0
             && ( nearest_teammate->pos().x > -30.0
                  || nearest_teammate->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point = nearest_teammate->inertiaFinalPoint();
            target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed ).execute( agent );
            agent->setNeckAction( new Neck_ScanField() );
            return;
        }
    }

    //
    // clear
    //

    if ( ( wm.ball().angleFromSelf() - wm.self().body() ).abs() > 1.5 )
    {
        agent->debugClient().addMessage( "FreeKick:Clear:TurnToBall" );
        dlog.addText( Logger::TEAM,
                      __FILE__":  clear. turn to ball" );

        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return;
    }

    agent->debugClient().addMessage( "FreeKick:Clear" );
    dlog.addText( Logger::TEAM,
                  __FILE__":  clear" );

    Body_ClearBall().execute( agent );
    agent->setNeckAction( new Neck_ScanField() );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
Bhv_SetPlayFreeKick::doKickWait( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doKickWait)" );


    const int real_set_play_count
        = static_cast< int >( wm.time().cycle() - wm.lastSetPlayStartTime().cycle() );

    if ( real_set_play_count >= ServerParam::i().dropBallTime() - 5 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) real set play count = %d > drop_time-10, force kick mode",
                      real_set_play_count );
        return false;
    }

    const Vector2D face_point( 40.0, 0.0 );
    const AngleDeg face_angle = ( face_point - wm.self().pos() ).th();

    if ( wm.time().stopped() != 0 )
    {
        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( Bhv_SetPlay::is_delaying_tactics_situation( agent ) )
    {
        agent->debugClient().addMessage( "FreeKick:Delaying" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) delaying" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.teammatesFromBall().empty() )
    {
        agent->debugClient().addMessage( "FreeKick:NoTeammate" );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no teammate" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() <= 3 )
    {
        agent->debugClient().addMessage( "FreeKick:Wait%d", wm.setplayCount() );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.setplayCount() >= 15
         && wm.seeTime() == wm.time()
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.6 )
    {
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) set play count = %d, force kick mode",
                      wm.setplayCount() );
        return false;
    }

    if ( ( face_angle - wm.self().body() ).abs() > 5.0 )
    {
        agent->debugClient().addMessage( "FreeKick:Turn" );

        Body_TurnToPoint( face_point ).execute( agent );
        agent->setNeckAction( new Neck_ScanField() );
        return true;
    }

    if ( wm.seeTime() != wm.time()
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.9 )
    {
        Body_TurnToBall().execute( agent );
        agent->setNeckAction( new Neck_ScanField() );

        agent->debugClient().addMessage( "FreeKick:Wait%d", wm.setplayCount() );
        dlog.addText( Logger::TEAM,
                      __FILE__": (doKickWait) no see or recover" );
        return true;
    }

    return false;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
Bhv_SetPlayFreeKick::doMove( PlayerAgent * agent )
{
    const WorldModel & wm = agent->world();

    dlog.addText( Logger::TEAM,
                  __FILE__": (doMove)" );

    Vector2D target_point = Strategy::i().getPosition( wm.self().unum() );

    if ( wm.setplayCount() > 0
         && wm.self().stamina() > ServerParam::i().staminaMax() * 0.9 )
    {
        const PlayerObject * nearest_opp = agent->world().getOpponentNearestToSelf( 5 );

        if ( nearest_opp && nearest_opp->distFromSelf() < 3.0 )
        {
            Vector2D add_vec = ( wm.ball().pos() - target_point );
            add_vec.setLength( 3.0 );

            long time_val = agent->world().time().cycle() % 60;
            if ( time_val < 20 )
            {

            }
            else if ( time_val < 40 )
            {
                target_point += add_vec.rotatedVector( 90.0 );
            }
            else
            {
                target_point += add_vec.rotatedVector( -90.0 );
            }

            target_point.x = min_max( - ServerParam::i().pitchHalfLength(),
                                      target_point.x,
                                      + ServerParam::i().pitchHalfLength() );
            target_point.y = min_max( - ServerParam::i().pitchHalfWidth(),
                                      target_point.y,
                                      + ServerParam::i().pitchHalfWidth() );
        }
    }

    target_point.x = std::min( target_point.x,
                               agent->world().offsideLineX() - 0.5 );

    double dash_power
        = Bhv_SetPlay::get_set_play_dash_power( agent );
    double dist_thr = wm.ball().distFromSelf() * 0.07;
    if ( dist_thr < 1.0 ) dist_thr = 1.0;

    agent->debugClient().addMessage( "SetPlayMove" );
    agent->debugClient().setTarget( target_point );
    agent->debugClient().addCircle( target_point, dist_thr );

    if ( ! Body_GoToPoint( target_point,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
    {
        // already there
        Body_TurnToBall().execute( agent );
    }

    if ( wm.self().pos().dist( target_point )
         > std::max( wm.ball().pos().dist( target_point ) * 0.2, dist_thr ) + 6.0
         || wm.self().stamina() < ServerParam::i().staminaMax() * 0.7 )
    {
        if ( ! wm.self().staminaModel().capacityIsEmpty() )
        {
            agent->debugClient().addMessage( "Sayw" );
            agent->addSayMessage( new WaitRequestMessage() );
        }
    }

    agent->setNeckAction( new Neck_TurnToBallOrScan() );
}

/*============================================================================================*/
        /*=======================  métodos estendidos  ===========================*/
/*============================================================================================*/

/*! Método é invocado em bhv_set_play.cpp, linha 172, dentro de execute() */
bool
Bhv_SetPlayFreeKick::jogada_coletiva_call( PlayerAgent * agent)
{
  static int jogada = 0;
  static int ciclo = 0; 
  
  if( (ciclo == 0 || ciclo+20 <= agent->world().time().cycle()) &&  Bhv_SetPlay::is_kicker( agent ) )
  {
  jogada = tomada_decisao(agent); //  <<<<<<<<--------------------------- TOMADA DE DECISÃO
    if(jogada == -1) // algum erro ocorreu ... 
    {
      execute(agent);
      ciclo = 0; 
      return false;
    }else      
       ciclo = agent->world().time().cycle();    
  }
  else if( ! Bhv_SetPlay::is_kicker( agent ))
  {
    if(agent->world().audioMemory().jogadaTime().cycle()>0) // exite uma mensagem de áudio
      jogada = agent->world().audioMemory().jogada().front().qual_jogada_;
    else
    {
      doMove(agent);
      return false; 
      
    }  
  }
 
    switch(jogada)
    {
    case 0:
      tabela_dir(agent);
      if(Bhv_SetPlay::is_kicker( agent ))
      std::cout << "\n tabela direita\n";
   case 1: //tabela pela esquerda
    tabela_esq(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n TABELA ESQ\n";
    break;
  case 2: //jogada de lateral com lançamento pela esquerda
    lateral_lanc_esq(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n LATERAL LANCAM. ESQ \n";
    break;
  case 3: //assistência chute pela esquerda
    assistencia_chute_esq(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n ASSISTENCIA ESQ \n";
    break;
  case 4: //inversão pela esquerda
    inversao_esq(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n INVERSAO ESQ \n";
    break;
  case 5://lancamento pelo meio
    meio_abertura_esq(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n MEIO AB. ESQ \n";
    break;
  case 6://meio aberto pelas laterais
    meio_abertura_dir(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n meio abertura direita\n";
    break;
  case 7://inversão direita
    inversao_dir(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n inversao direita\n";
    break;
  case 8://assistência para chute pela direita
    assistencia_chute_dir(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n assistencia direita\n";
    break;
  case 9://jogada pela lateral com lançamento pela direita
    lateral_lanc_dir(agent);
    if(Bhv_SetPlay::is_kicker( agent ))
    std::cout << "\n lateral lancamento direita\n";
    break;
  default:
    std::cout << "\n XXXXXXXXXXXXXXXXXXXXX ERROR XXXXXXXXXXXXXXXXXXXXXXXXXXXXX \n";
      }
  
  
  
   // assistencia_chute_esq(agent);
  //  meio_abertura_dir(agent);
   //lateral_lanc_dir(agent);
 
 
 // inversao_dir(agent); 
  
  //tabela_esq(agent);
  
  
 // execute(agent);
  return true;
  
  
}

//====================================================================//

void Bhv_SetPlayFreeKick::passToPlayer(PlayerAgent* agent, bool jogada_direita)
{
  static
  int num_jog = 0, num1=0, num2=0;
  //static
  Vector2D ponto_desejado1, ponto_desejado2;
  
  if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    { // vá para o ponto onde a bola está parada...
         return;
    }

    //
    // aguarde...
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // É hora de chutar!
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    //
    // passe - muito mal sucedido.
    //
    //if ( Bhv_ChainAction().execute( agent ) )
   // {
   //     agent->debugClient().addMessage( "FreeKick:Chain" );
   //     agent->setIntention( new IntentionWaitAfterSetPlayKick() );
    //    return;
   // }
  
    // chutar para o aliado destinatário da mensagem...
    //
    
    const PlayerObject * nearest_teammate = agent->world().getTeammateNearestToSelf( 10, false );
    /* pegar o jogador que está mais próximo do ponto, em vez do que está mais próximo do cobrador! */
    num_jog = nearest_teammate->unum();
     
    /*
     * Antes de definir os pontos, falta ainda verificar os casos em que ocorrem impedimentos...
     * caso isso ocorra, ou sejam detectados adversários a menos de 1 ou 1.5 m do ponto-destino
     * as constantes que dão os pontos-destino em relação à bola devem ser alteradas!
     */
    
     Vector2D jog2,jog ;	 
     double x,y;	      
     
	    if(jogada_direita)
	    {
		 //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		 x =agent->world().ball().pos().x + 2*VALOR_X;
		 y = agent->world().ball().pos().y + 2*VALOR_Y;	
		
	    }
	    else
	    {
	       //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		x = agent->world().ball().pos().x + 2*VALOR_X; // 5 metros a frente
		y = agent->world().ball().pos().y + 2*VALOR_Y*(-1); // 2 metros de lado  
	    }
                if(x<=agent->world().offsideLineX())
                  jog.x = (x>=52.0)?52.0:x; 
		else
		  jog.x = agent->world().offsideLineX();
	
		if(y>-34.0)
		  jog.y = (y>=34)?34:y;  
		else
		  jog.y = -34;
        x = y = 0;    
		
    ponto_desejado1 = jog;
	   
            if(jogada_direita)
	    {
		x = agent->world().ball().pos().x + VALOR_X*3;
		y = agent->world().ball().pos().y -( VALOR_Y*3 );
	    }
	    else
	    {
	      x = agent->world().ball().pos().x + (VALOR_X*3);
	      y = agent->world().ball().pos().y + (VALOR_Y*3);
	    }
	      if(x<=agent->world().offsideLineX())
	        jog2.x = (x>=52)?52:x;  
	      else
		jog2.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog2.y = (y>=34)?34:y;  
		else
		  jog2.y = -34;
        
    ponto_desejado2 = jog2;		
        
    //double dist_from_ball = 100.0;
    double dist_from_point1 = 100.0, dist_from_point2 = 100.0;
     
    //procura o  agente mais próximo do ponto de destino ... (que não seja ele próprio, o goleiro
    const AbstractPlayerCont::const_iterator t_end = agent->world().allTeammates().end();
    for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	  //  if ( (*t)->unum() == nearest_teammate->unum() ) continue;
	    
	   if( (*t)->pos().dist(jog) < dist_from_point1)
	   {
	    dist_from_point1 = (*t)->pos().dist(jog);
	    num1 = (*t)->unum();
	   }	     
	  }
	  
	  //procura o agente mais próximo do segundo ponto de destino, que não seja ele próprio, o agente anterior ou o goleiro...
	   for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	    if( (*t)->unum() == num1 ) continue;
	    
	   if( (*t)->pos().dist(jog2) < dist_from_point2)
	   {
	    dist_from_point2 = (*t)->pos().dist(jog2);
	    num2 = (*t)->unum();
	   }
	     
	  }
	 
	 // garantindo que os números dos jogadores enviados sejam diferentes de NULL
	 if( (num1 == -1) || (num2==-1) )
	 {
	    const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false );
	   if (num1 ==-1)
	     num1 = nearest_teammate->unum();
	   else if(num2==-1 && num1 != nearest_teammate->unum())
	     num2 = nearest_teammate->unum();
	   
	  if (num1==-1)
	     for( int i=11; i>=2;i--)
	     {	       
	       if (i == wm.self().unum() ) continue;
	       
	       num1 = i; break;
	     }
	   
	   if (num2==-1)
	     for( int i=11; i>=2;i--)
	     {
	       if (i == num1) continue;
	       if (i == wm.self().unum() ) continue;
	       
	       num2 = i; break;
	     }
	     
	 }
	 
	 
     // quantos ciclos o aliado vai levar pra chegar ao ponto que pedi para ele ir ?
     int ciclo_aliado = wm.teammate(num1)->playerTypePtr()->cyclesToReachDistance
      ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) );
      
    if((ciclo_aliado > 2) || ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) > 2.0) )//se o aliado que vai receber a bola estah longe ainda...
    {
      //espera o jogador que vai participar da jogada chegar ao ponto... enquanto isso envi outra mensagem para mantê-lo guiado !
    
	  rcsc::jogadas jogada_atual = (jogada_direita)?(rcsc::tabela_dir):(rcsc::tabela_esq);
    
	 agent->addSayMessage( new JogadaMessage
		  (jogada_atual,ponto_desejado1, ponto_desejado2, num1, num2) ); 	
	  Bhv_GoToStaticBall( 0.0 ).execute( agent );// Body_ClearBall().execute( agent );
          agent->setNeckAction( new Neck_ScanField() );
	//  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	//  std:: cout         << "\nagente 1 = "<< num1 
	//	             <<"\nagente 2  = " << num2
	//	             <<"\nsender = " << agent->world().self().unum();
	 
	    
	  return;
	
	//envia outra mensagem, se não, cobra a falta
      }else
      {
      
	num_jog = wm.audioMemory().jogada().front().j1_;
      
    // std::cout << "\nchutei para o player = " << num_jog  << "\n\n";
    
   // const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if (  (wm.teammate(num_jog)->distFromSelf()< 20.0
	      &&  wm.teammate(num_jog)->pos().x > -30)
                  ||(  wm.teammate(num_jog)->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point =   wm.teammate(num_jog)->inertiaFinalPoint(); //nearest_teammate->inertiaFinalPoint();
            target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed*0.8 ).execute( agent );
	    agent->setNeckAction( new Neck_ScanField() );
    
    
    num_jog = 0;
    ponto_desejado1.x =0;
    ponto_desejado1.y =0;
	}
    return;
    //}
    }
    
  
}

//====================================================================//

bool
Bhv_SetPlayFreeKick::tabela_dir(PlayerAgent * agent){
  // unum tem o número do uniforme do agente
  
  const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      passToPlayer(agent, true); //argumeto "true" indica que é tabela pela direita
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	   //static int cic = wm.audioMemory().jogadaTime().cycle() ;
	    // agent->world().self().type().
	     //PlayerType tipo;
	     
	     //comparar quantos ciclos o agente vai levar pra chegar ao ponto com o ciclo atual  agent->world()
	    
		double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {
		
		
		//std::cout << "\ndestino 1, numero = " << agent->world().self().unum()<<"\n"
		//<< "estou no ponto = ( "<< agent->world().self().pos().x<<" , "<<agent->world().self().pos().y
		//<< " )\ntenho que ir para o ponto = ( " << wm.audioMemory().jogada().front().jog1_pos_.x
		//<<" , "<< wm.audioMemory().jogada().front().jog1_pos_.y	<<")\n";
		
		//Arm_PointToPoint(wm.audioMemory().jogada().front().jog1_pos_ ).execute(agent);
		//Bhv_NeckBodyToPoint(wm.audioMemory().jogada().front().jog1_pos_ ).execute(agent);
		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 
		//std::cout << "\ndestino 2, numero = " << agent->world().self().unum()<<"\n";
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true;  
  
}

//============================================================//

bool
Bhv_SetPlayFreeKick::tabela_esq(PlayerAgent * agent){
  // unum tem o número do uniforme do agente
  
  const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      passToPlayer(agent, false); // argumento "false" indica que tabela é pela esquerda
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
		double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
				
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 
		//std::cout << "\ndestino 2, numero = " << agent->world().self().unum()<<"\n";
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true;  
  
}

//===============================================================================
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

bool 
Bhv_SetPlayFreeKick::meio_abertura_dir(rcsc::PlayerAgent * agent)
{
  const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      aberturaToPlayer(agent, true); //argumeto "true" indica que é abertura pela direita
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	   //static int cic = wm.audioMemory().jogadaTime().cycle() ;
	    // agent->world().self().type().
	     //PlayerType tipo;
	     
	     //comparar quantos ciclos o agente vai levar pra chegar ao ponto com o ciclo atual  agent->world()
	    
		double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {
		
		
		//std::cout << "\ndestino 1, numero = " << agent->world().self().unum()<<"\n"
		//<< "estou no ponto = ( "<< agent->world().self().pos().x<<" , "<<agent->world().self().pos().y
		//<< " )\ntenho que ir para o ponto = ( " << wm.audioMemory().jogada().front().jog1_pos_.x
		//<<" , "<< wm.audioMemory().jogada().front().jog1_pos_.y	<<")\n";
		
		//Arm_PointToPoint(wm.audioMemory().jogada().front().jog1_pos_ ).execute(agent);
		//Bhv_NeckBodyToPoint(wm.audioMemory().jogada().front().jog1_pos_ ).execute(agent);
		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 
		//std::cout << "\ndestino 2, numero = " << agent->world().self().unum()<<"\n";
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true; 
  
  
}

//========================================================================================

void Bhv_SetPlayFreeKick::aberturaToPlayer(PlayerAgent* agent, bool jogada_direita)
{
   static
  int num_jog = 0, num1=0, num2=0;
  //static
  Vector2D ponto_desejado1, ponto_desejado2;
  
  if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    { // vá para o ponto onde a bola está parada...
         return;
    }

    //
    // aguarde...
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // É hora de chutar!
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

     
    // chutar para o aliado destinatário da mensagem...
 
    /*
     * Antes de definir os pontos, falta ainda verificar os casos em que ocorrem impedimentos...
     * caso isso ocorra, ou sejam detectados adversários a menos de 1 ou 1.5 m do ponto-destino
     * as constantes que dão os pontos-destino em relação à bola devem ser alteradas!
     */
    
     Vector2D jog2,jog ;	 
	double x,y;      
	
		 //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		x = agent->world().ball().pos().x + 2*VALOR_X; // 10 metros a frente
		y = agent->world().ball().pos().y;   
	     
		if(agent->world().ball().pos().x > 40.0)
		{		   
			x = agent->world().ball().pos().x - 2*VALOR_X; // 10 metros atrás
		        y = agent->world().ball().pos().y;		  
		}
		
         if(x<=agent->world().offsideLineX())
                  jog.x = (x>=52)?52:x; 
		else
		  jog.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog.y = (y>=34)?34:y;  
		else
		  jog.y = -34;
        x = y = 0;    
        
    ponto_desejado1 = jog;
	 
    
           if(agent->world().ball().pos().x > 40.0)
	  {
	        if(jogada_direita)
		{	         
		    x = agent->world().ball().pos().x - 3*VALOR_X - 4;
		    y = agent->world().ball().pos().y + 4*VALOR_Y ;
		}
		else
		{
		  x = agent->world().ball().pos().x - (3*VALOR_X - 4);
		  y = agent->world().ball().pos().y + (4*VALOR_Y )*(-1);
		}
	  }     
           else
	   {
		if(jogada_direita)
		{	         
		    x = agent->world().ball().pos().x + 3*VALOR_X + 4;
		    y = agent->world().ball().pos().y + 4*VALOR_Y ;
		}
		else
		{
		  x = agent->world().ball().pos().x + (3*VALOR_X + 4);
		  y = agent->world().ball().pos().y + (4*VALOR_Y )*(-1);
		}
	   }
	   
	   
    if(x<=agent->world().offsideLineX())
                  jog2.x = (x>=52)?52:x; 
		else
		  jog2.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog2.y = (y>=34)?34:y;  
		else
		  jog2.y = -34;
        
    ponto_desejado2 = jog2;		
        
    //double dist_from_ball = 100.0;
    double dist_from_point1 = 100.0, dist_from_point2 = 100.0;
     
    //procura o  agente mais próximo do ponto de destino ... (que não seja ele próprio, o goleiro
    const AbstractPlayerCont::const_iterator t_end = agent->world().allTeammates().end();
    for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	  //  if ( (*t)->unum() == nearest_teammate->unum() ) continue;
	    
	   if( (*t)->pos().dist(jog) < dist_from_point1)
	   {
	    dist_from_point1 = (*t)->pos().dist(jog);
	    num1 = (*t)->unum();
	   }	     
	  }
	  
	  //procura o agente mais próximo do segundo ponto de destino, que não seja ele próprio, o agente anterior ou o goleiro...
	   for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	    if( (*t)->unum() == num1 ) continue;
	    
	   if( (*t)->pos().dist(jog2) < dist_from_point2)
	   {
	    dist_from_point2 = (*t)->pos().dist(jog2);
	    num2 = (*t)->unum();
	   }
	     
	  }
	 
	  // garantindo que os números dos jogadores enviados sejam diferentes de NULL
	 if( (num1 == -1) || (num2==-1) )
	 {
	    const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false );
	   if (num1 ==-1)
	     num1 = nearest_teammate->unum();
	   else if(num2==-1 && num1 != nearest_teammate->unum())
	     num2 = nearest_teammate->unum();
	   
	  if (num1==-1)
	     for( int i=11; i>=2;i--)
	     {	       
	       if (i == wm.self().unum() ) continue;
	       
	       num1 = i; break;
	     }
	   
	   if (num2==-1)
	     for( int i=11; i>=2;i--)
	     {
	       if (i == num1) continue;
	       if (i == wm.self().unum() ) continue;
	       
	       num2 = i; break;
	     }
	     
	 }
	 
     // quantos ciclos o aliado vai levar pra chegar ao ponto que pedi para ele ir ?
     int ciclo_aliado = wm.teammate(num1)->playerTypePtr()->cyclesToReachDistance
      ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) );
      
    if(((ciclo_aliado > 2) || ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) > 2.0))
      || wm.teammate(num2)->pos().dist(ponto_desejado2) > 5.0
    )//se o aliado que vai receber a bola estah longe ainda...
    {
      //espera o jogador que vai participar da jogada chegar ao ponto... enquanto isso envi outra mensagem para mantê-lo guiado !
    
	rcsc::jogadas jogada_atual = (jogada_direita)?(rcsc::meio_aberto_lateral_dir):(rcsc::meio_aberto_lateral_esq);
    
	 agent->addSayMessage( new JogadaMessage
		  (jogada_atual ,ponto_desejado1, ponto_desejado2, num1, num2) ); 	
	  Bhv_GoToStaticBall( 0.0 ).execute( agent );// Body_ClearBall().execute( agent );
          agent->setNeckAction( new Neck_ScanField() );
	
	  return;
	
	//envia outra mensagem, se não, cobra a falta
      }else
      {
      
	num_jog = wm.audioMemory().jogada().front().j1_;
      
    // std::cout << "\nchutei para o player = " << num_jog  << "\n\n";
    
   // const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if (  (wm.teammate(num_jog)->distFromSelf()< 20.0
	      &&  wm.teammate(num_jog)->pos().x > -30)
                  ||(  wm.teammate(num_jog)->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point =   wm.teammate(num_jog)->inertiaFinalPoint(); //nearest_teammate->inertiaFinalPoint();
            target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed ).execute( agent );
	    agent->setNeckAction( new Neck_ScanField() );
    
    
    num_jog = 0;
    ponto_desejado1.x =0;
    ponto_desejado1.y =0;
	}
    return;
    //}
    }
  
  
}

//==========================================================================================

 bool Bhv_SetPlayFreeKick::meio_abertura_esq(rcsc::PlayerAgent * agent)
 {
   const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      aberturaToPlayer(agent, false); //argumeto "false" indica que é abertura será realizada pela esquerda
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	    double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {			
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 	
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true; 
 }
 
//======================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


 bool Bhv_SetPlayFreeKick::lateral_lanc_dir(rcsc::PlayerAgent * agent)
 {
     const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      centerToPlayer(agent, true); //argumeto "true" indica que é abertura será realizada pela direita
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	    double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {			
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 	
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true; 
}

//==========================================================================================

bool Bhv_SetPlayFreeKick::lateral_lanc_esq(rcsc::PlayerAgent * agent)
{
  const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      centerToPlayer(agent, false); //argumeto "false" indica que é abertura será realizada pela esquerda
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	    double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {			
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 	
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true; 
}
     
//==========================================================================================

void Bhv_SetPlayFreeKick::centerToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita)
{
   static
  int num_jog = 0, num1=0, num2=0;
  //static
  Vector2D ponto_desejado1, ponto_desejado2;
  
  if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    { // vá para o ponto onde a bola está parada...
         return;
    }

    //
    // aguarde...
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // É hora de chutar!
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

     
    // chutar para o aliado destinatário da mensagem...
 
    /*
     * Antes de definir os pontos, falta ainda verificar os casos em que ocorrem impedimentos...
     * caso isso ocorra, ou sejam detectados adversários a menos de 1 ou 1.5 m do ponto-destino
     * as constantes que dão os pontos-destino em relação à bola devem ser alteradas!
     */
    
     Vector2D jog2,jog ;	 
      double x,y;	      
	   
	    if(agent->world().ball().pos().x > 45 )
	    {
	      if(jogada_direita)
		{
		    x = agent->world().ball().pos().x - VALOR_X;
		    y = agent->world().ball().pos().y + 4*VALOR_Y;
		}
		else
		{
		    x = agent->world().ball().pos().x - (VALOR_X);
		    y = agent->world().ball().pos().y + (4*VALOR_Y)*(-1);
		}
	      
	    }
            else
	    {  
	      if(jogada_direita)
		{
		    x = agent->world().ball().pos().x + VALOR_X;
		    y = agent->world().ball().pos().y + 4*VALOR_Y;
		}
		else
		{
		    x = agent->world().ball().pos().x + (VALOR_X);
		    y = agent->world().ball().pos().y + (4*VALOR_Y)*(-1);
		}
	    }     
         if(x<=agent->world().offsideLineX())
                  jog.x = (x>=52)?52:x; 
		else
		  jog.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog.y = (y>=34)?34:y;  
		else
		  jog.y = -34;
        x = y = 0;    
        
    ponto_desejado1 = jog;
	   
		if (agent->world().ball().pos().x > 45.0)
		{
		// posição para onde o aliado deve ir para começar a jogada coletiva
		    x = agent->world().ball().pos().x - 2*VALOR_X - 2; // 5 metros a frente
		    y = agent->world().ball().pos().y;   
		}  
		else{
		  //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		    x = agent->world().ball().pos().x + 2*VALOR_X + 2; // 5 metros a frente
		    y = agent->world().ball().pos().y;  
		 }
		    
               if(x<=agent->world().offsideLineX())
                  jog2.x = (x>=52)?52:x; 
		else
		  jog2.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog2.y = (y>=34)?34:y;  
		else
		  jog2.y = -34;
      		
    ponto_desejado2 = jog2;		
        
    //double dist_from_ball = 100.0;
    double dist_from_point1 = 100.0, dist_from_point2 = 100.0;
     
    //procura o  agente mais próximo do ponto de destino ... (que não seja ele próprio, o goleiro
    const AbstractPlayerCont::const_iterator t_end = agent->world().allTeammates().end();
    for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	  //  if ( (*t)->unum() == nearest_teammate->unum() ) continue;
	    
	   if( (*t)->pos().dist(jog) < dist_from_point1)
	   {
	    dist_from_point1 = (*t)->pos().dist(jog);
	    num1 = (*t)->unum();
	   }	     
	  }
	  
	  //procura o agente mais próximo do segundo ponto de destino, que não seja ele próprio, o agente anterior ou o goleiro...
	   for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	    if( (*t)->unum() == num1 ) continue;
	    
	   if( (*t)->pos().dist(jog2) < dist_from_point2)
	   {
	    dist_from_point2 = (*t)->pos().dist(jog2);
	    num2 = (*t)->unum();
	   }
	     
	  }
	  // garantindo que os números dos jogadores enviados sejam diferentes de NULL
	 if( (num1 == -1) || (num2==-1) )
	 {
	    const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false );
	   if (num1 ==-1)
	     num1 = nearest_teammate->unum();
	   else if(num2==-1 && num1 != nearest_teammate->unum())
	     num2 = nearest_teammate->unum();
	   
	  if (num1==-1)
	     for( int i=11; i>=2;i--)
	     {	       
	       if (i == wm.self().unum() ) continue;
	       
	       num1 = i; break;
	     }
	   
	   if (num2==-1)
	     for( int i=11; i>=2;i--)
	     {
	       if (i == num1) continue;
	       if (i == wm.self().unum() ) continue;
	       
	       num2 = i; break;
	     }
	     
	 }
	 
     // quantos ciclos o aliado vai levar pra chegar ao ponto que pedi para ele ir ?
     int ciclo_aliado = wm.teammate(num1)->playerTypePtr()->cyclesToReachDistance
      ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) );
      
    if(((ciclo_aliado > 2) || ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) > 2.0))
      || ( wm.teammate(num2)->pos().dist(ponto_desejado2) > 5.0)
    )//se o aliado que vai receber a bola estah longe ainda...
    {
      //espera o jogador que vai participar da jogada chegar ao ponto... enquanto isso envi outra mensagem para mantê-lo guiado !
    
	rcsc::jogadas jogada_atual = (jogada_direita)?(rcsc::lateral_lanc_dir):(rcsc::lateral_lanc_esq);
    
	 agent->addSayMessage( new JogadaMessage
		  (jogada_atual ,ponto_desejado1, ponto_desejado2, num1, num2) ); 	
	  Bhv_GoToStaticBall( 0.0 ).execute( agent );// Body_ClearBall().execute( agent );
          agent->setNeckAction( new Neck_ScanField() );
	
	  return;
	
	//envia outra mensagem, se não, cobra a falta
      }else
      {
      
	num_jog = wm.audioMemory().jogada().front().j1_;
      
    // std::cout << "\nchutei para o player = " << num_jog  << "\n\n";
    
   // const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if (  (wm.teammate(num_jog)->distFromSelf()< 20.0
	      &&  wm.teammate(num_jog)->pos().x > -30)
                  ||(  wm.teammate(num_jog)->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point =   wm.teammate(num_jog)->inertiaFinalPoint(); //nearest_teammate->inertiaFinalPoint();
         //   target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );
	    ball_speed = ball_speed*0.85;

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed ).execute( agent );
	    agent->setNeckAction( new Neck_ScanField() );
    
    
    num_jog = 0;
    ponto_desejado1.x =0;
    ponto_desejado1.y =0;
	}
    return;
    
    }
  
}

//======================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool Bhv_SetPlayFreeKick::inversao_dir(rcsc::PlayerAgent * agent)
{
      const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      inversaoToPlayer(agent, true); //argumeto "true" indica que é abertura será realizada pela direita
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 10 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 10 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	    double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {			
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		//std::cout << "\n jogador 1 = "<< wm.audioMemory().jogada().front().j1_
		  //        << "\n jogador 2 = " << wm.audioMemory().jogada().front().j2_
		    //      << "\n sender = " << wm.audioMemory().jogada().front().sender_ << "\n";
		
		//if(wm.self().isKickable()) // dá pra chutar a bola?
		  
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 	
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);

  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true; 
}
 
//==========================================================================================

bool Bhv_SetPlayFreeKick::inversao_esq(rcsc::PlayerAgent * agent)
{
    const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      inversaoToPlayer(agent, false); //argumeto "false" indica que é abertura será realizada pela esquerda
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 8 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 8 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
	    double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {			
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		//std::cout << "\n jogador 1 = "<< wm.audioMemory().jogada().front().j1_
		  //        << "\n jogador 2 = " << wm.audioMemory().jogada().front().j2_
		    //      << "\n sender = " << wm.audioMemory().jogada().front().sender_ << "\n";
		
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 	
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_ScanField() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_ScanField() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true; 
}
     
//==========================================================================================

void Bhv_SetPlayFreeKick::inversaoToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita)
{
     static
  int num_jog = 0, num1=0, num2=0;
  //static
  Vector2D ponto_desejado1, ponto_desejado2;
  
  if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    { // vá para o ponto onde a bola está parada...
         return;
    }

    //
    // aguarde...
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // É hora de chutar!
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

     
    // chutar para o aliado destinatário da mensagem...
 
    /*
     * Antes de definir os pontos, falta ainda verificar os casos em que ocorrem impedimentos...
     * caso isso ocorra, ou sejam detectados adversários a menos de 1 ou 1.5 m do ponto-destino
     * as constantes que dão os pontos-destino em relação à bola devem ser alteradas!
     */
    
     Vector2D jog2,jog ;	 
     double x,y;	      
	   
             if(agent->world().ball().pos().x>= 43.0)
	     {
	      
	       if(jogada_direita)
		{
		    x = agent->world().ball().pos().x - VALOR_X - 2;
		    y = agent->world().ball().pos().y + 5*VALOR_Y;
		}
		else
		{
		    x = agent->world().ball().pos().x - VALOR_X -2;
		    y = agent->world().ball().pos().y + (5*VALOR_Y)*(-1);
		}
	       
	    }  
            else
	    {  
		if(jogada_direita)
		{
		    x = agent->world().ball().pos().x - VALOR_X;
		    y = agent->world().ball().pos().y + 5*VALOR_Y;
		}
		else
		{
		    x = agent->world().ball().pos().x - VALOR_X;
		    y = agent->world().ball().pos().y + (5*VALOR_Y)*(-1);
		}
	    
	      
	    }
	    
             if(x<=agent->world().offsideLineX())
                  jog.x = (x>=52.0)?52.0:x; 
		else
		  jog.x = agent->world().offsideLineX();
	
		if(y>-34.0)
		  jog.y = (y>=34.0)?34.0:y;  
		else
		  jog.y = -34;
        x = y = 0.0;    
        
    ponto_desejado1 = jog;
	   
    
             if(agent->world().ball().pos().x>= 43.0)
	     {
	      
	        //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		  if(jogada_direita)
		  {
		      x = agent->world().ball().pos().x - 2*VALOR_X;
		      y = agent->world().ball().pos().y + 8*VALOR_Y;
		  }
		  else
		  {
		      x = agent->world().ball().pos().x - 2*VALOR_X;
		      y = agent->world().ball().pos().y + (8*VALOR_Y)*(-1);
		  } 
	       
	    }  
            else
	    {    
    
		  //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		  if(jogada_direita)
		  {
		      x = agent->world().ball().pos().x + 2*VALOR_X;
		      y = agent->world().ball().pos().y + 8*VALOR_Y;
		  }
		  else
		  {
		      x = agent->world().ball().pos().x + 2*VALOR_X;
		      y = agent->world().ball().pos().y + (8*VALOR_Y)*(-1);
		  } 
	    }
	    
	    if(x<=agent->world().offsideLineX())
                  jog2.x = (x>=52)?52:x; 
		else
		  jog2.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog2.y = (y>=34)?34:y;  
		else
		  jog2.y = -34;
   
    ponto_desejado2 = jog2;		
        
    //double dist_from_ball = 100.0;
    double dist_from_point1 = 100.0, dist_from_point2 = 100.0;
     
    //procura o  agente mais próximo do ponto de destino ... (que não seja ele próprio, o goleiro
    const AbstractPlayerCont::const_iterator t_end = agent->world().allTeammates().end();
    for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	  //  if ( (*t)->unum() == nearest_teammate->unum() ) continue;
	    
	   if( (*t)->pos().dist(jog) < dist_from_point1)
	   {
	    dist_from_point1 = (*t)->pos().dist(jog);
	    num1 = (*t)->unum();
	   }	     
	  }
	  
	  //procura o agente mais próximo do segundo ponto de destino, que não seja ele próprio, o agente anterior ou o goleiro...
	   for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	    if( (*t)->unum() == num1 ) continue;
	    
	   if( (*t)->pos().dist(jog2) < dist_from_point2)
	   {
	    dist_from_point2 = (*t)->pos().dist(jog2);
	    num2 = (*t)->unum();
	   }
	     
	  }
	 
	  // garantindo que os números dos jogadores enviados sejam diferentes de NULL
	 if( (num1 == -1) || (num2==-1) )
	 {
	    const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false );
	   if (num1 ==-1)
	     num1 = nearest_teammate->unum();
	   else if(num2==-1 && num1 != nearest_teammate->unum())
	     num2 = nearest_teammate->unum();
	   
	  if (num1==-1)
	     for( int i=11; i>=2;i--)
	     {	       
	       if (i == wm.self().unum() ) continue;
	       
	       num1 = i; break;
	     }
	   
	   if (num2==-1)
	     for( int i=11; i>=2;i--)
	     {
	       if (i == num1) continue;
	       if (i == wm.self().unum() ) continue;
	       
	       num2 = i; break;
	     }
	     
	 }
	 
     // quantos ciclos o aliado vai levar pra chegar ao ponto que pedi para ele ir ?
     int ciclo_aliado = wm.teammate(num1)->playerTypePtr()->cyclesToReachDistance
      ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) );
      
    if(((ciclo_aliado > 2) || ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) > 2.0))
      || ( wm.teammate(num2)->pos().dist(ponto_desejado2) > 5.0)
    )//se o aliado que vai receber a bola estah longe ainda...
    {
      //espera o jogador que vai participar da jogada chegar ao ponto... enquanto isso envi outra mensagem para mantê-lo guiado !
    
	rcsc::jogadas jogada_atual = (jogada_direita)?(rcsc::inversao_dir):(rcsc::inversao_esq);
    
	 agent->addSayMessage( new JogadaMessage
		  (jogada_atual ,ponto_desejado1, ponto_desejado2, num1, num2) ); 	
	  Bhv_GoToStaticBall( 0.0 ).execute( agent );// Body_ClearBall().execute( agent );
          agent->setNeckAction( new Neck_ScanField() );
	
	  return;
	
	//envia outra mensagem, se não, cobra a falta
      }else
      {
      
	num_jog = wm.audioMemory().jogada().front().j1_;
      
    // std::cout << "\nplayer 1 = " << num_jog  << "\nplayer 2 = " << wm.audioMemory().jogada().front().j2_
    // << "\nsender = " << wm.self().unum() <<"\n\n";
    
   // const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if (  (wm.teammate(num_jog)->distFromSelf()< 20.0
	      &&  wm.teammate(num_jog)->pos().x > -30)
                  ||(  wm.teammate(num_jog)->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point =   wm.teammate(num_jog)->inertiaFinalPoint(); //nearest_teammate->inertiaFinalPoint();
           // target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );
	  //  ball_speed = ball_speed*0.7;

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed ).execute( agent );
	    agent->setNeckAction( new Neck_ScanField() );
    
    
    num_jog = 0;
    ponto_desejado1.x =0;
    ponto_desejado1.y =0;
	}
    return;
    
    }
} 

//======================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     
bool Bhv_SetPlayFreeKick::assistencia_chute_dir(rcsc::PlayerAgent * agent)
  {
       const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      assistenciaToPlayer(agent, true); // argumento "true" indica que assistência é pela direita
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
		double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
				
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 
		//std::cout << "\ndestino 2, numero = " << agent->world().self().unum()<<"\n";
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true;    
  }
//==========================================================================================
     
    
bool Bhv_SetPlayFreeKick::assistencia_chute_esq(rcsc::PlayerAgent * agent)
{
    const WorldModel & wm = agent->world() ;
   int num, num_final=0;
   double menor_ball_dist=100;
   
   const PlayerObject * aliado_mais_proximo = agent->world().getTeammateNearestToSelf(10, false); //até uma distância de 10 metros
 	if ( (aliado_mais_proximo->unum() < 1 )|| (aliado_mais_proximo->unum()>11) )
	   { execute(agent); return false; } //não é um teammate válido
   
  //encontrar o agente mais próximo da bola
  for(num=1;num<=11;num++)
  {
    if ( num == wm.teammateGoalieUnum() ) continue; //se for o goleiro continuar o loop

        Vector2D pos_agent = Strategy::i().getPosition( num );
        if ( ! pos_agent.isValid() ) continue; //se não for uma posição válida
    
     if( pos_agent.dist(wm.ball().pos()) < menor_ball_dist)//se a distâcia do agente até a bola for a menor, ele é o mais próximo...
       {
	 num_final = num;
	 menor_ball_dist = pos_agent.dist(wm.ball().pos());
       }
  }
  
  //você é o agente que vai cobrar a bola parada...
  if ( Bhv_SetPlay::is_kicker( agent ) ||  //&&
     ((num_final != 0) && 
     (wm.self().unum()==num_final) ) ) // se sou o agente mais próximo da bola...
    { 
   
      assistenciaToPlayer(agent, false); // argumento "false" indica que tabela é pela esquerda
     agent->setNeckAction( new Neck_TurnToBallOrScan() );
      return true;
      	
    }
    else
    { 
      
      if(aliado_mais_proximo == ( static_cast< PlayerObject * >( 0 ) )  ) //se for igual que um ponteiro nulo
      { execute(agent);  return false; } //se não houver nenhum player próximo à bola 
      else //mantenha o foco da atenção no agente aliado que irá tirar a bola...
      agent->doAttentionto(wm.ourSide(), aliado_mais_proximo->unum() );
      
    //se o jogador ouviu uma mensagem de áudio nos 2 últimos ciclos 
        if( ( (wm.audioMemory().jogadaTime().cycle() != -1) && 
	  ( ( wm.audioMemory().jogadaTime().cycle() + 2 ) >= agent->world().time().cycle())) )
	{ 	
	  //verifica se ele é um dos destinatários da mensagem
	  if( ( (wm.audioMemory().jogada().front().j1_ >=1) &&
	      ( wm.audioMemory().jogada().front().j1_<=11 )) && (
	    ( wm.audioMemory().jogada().front().j1_ == agent->world().self().unum() ) 
	  || (  wm.audioMemory().jogada().front().j2_ == agent->world().self().unum()))  )//sou algum dos destinatários da mensagem ?
	  { 
		double dash_power
	      = Bhv_SetPlay::get_set_play_dash_power( agent ); // captura a potencia de velocidade do agente
		
	       double dist_thr;
	      if(  wm.audioMemory().jogada().front().j1_ == wm.self().unum() ) // se você é o primeiro jogador do qual a mensagem trata
	      {		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog1_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog1_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog1_pos_ ).execute( agent );
		}
				
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		   
		  return true;
		  
	      }
	      else
	      { 
		//std::cout << "\ndestino 2, numero = " << agent->world().self().unum()<<"\n";
		
		// quantos ciclos o agente vai levar para chegar até a posição especificada pelo remetente...		
		dist_thr = wm.self().pos().dist(wm.audioMemory().jogada().front().jog2_pos_) * 0.07;
		if ( dist_thr < 0.5 ) dist_thr = 0.5;
		
		//tenta se mover paa o ponto, se não conseguir tem que corrigir o ângulo para onde seu corpo está voltado
		if ( ! Body_GoToPoint( wm.audioMemory().jogada().front().jog2_pos_,
                           dist_thr,
                           dash_power
                           ).execute( agent ) )
		{
		  Body_TurnToPoint( wm.audioMemory().jogada().front().jog2_pos_ ).execute( agent );
		}
		
		    agent->setNeckAction( new Neck_TurnToBallOrScan() );
		     return true;
		      }
		 agent->setNeckAction( new Neck_TurnToBallOrScan() );
		      return true;
	}else
	{	
	  doMove( agent );
	  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  return true;
	}
	  
	}
	       
    }
  doMove(agent);
  //Bhv_SetPlay:: doBasicTheirSetPlayMove( agent ); //vá para sua posição inicial
  Body_TurnToBall().execute( agent );
  agent->setNeckAction( new Neck_TurnToBallOrScan() );
  return true;  
}
  
//==========================================================================================
     
     
void Bhv_SetPlayFreeKick::assistenciaToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita)
{
   static
  int num_jog = 0, num1=0, num2=0;
  //static
  Vector2D ponto_desejado1, ponto_desejado2;
  
  if ( Bhv_GoToStaticBall( 0.0 ).execute( agent ) )
    { // vá para o ponto onde a bola está parada...
         return;
    }

    //
    // aguarde...
    //

    if ( doKickWait( agent ) )
    {
        return;
    }

    //
    // É hora de chutar!
    //

    const WorldModel & wm = agent->world();
    const double max_ball_speed = wm.self().kickRate() * ServerParam::i().maxPower();

    //
    // passe - muito mal sucedido.
    //
    //if ( Bhv_ChainAction().execute( agent ) )
   // {
   //     agent->debugClient().addMessage( "FreeKick:Chain" );
   //     agent->setIntention( new IntentionWaitAfterSetPlayKick() );
    //    return;
   // }
  
    /*
     * Antes de definir os pontos, falta ainda verificar os casos em que ocorrem impedimentos...
     * caso isso ocorra, ou sejam detectados adversários a menos de 1 ou 1.5 m do ponto-destino
     * as constantes que dão os pontos-destino em relação à bola devem ser alteradas!
     */
    
     Vector2D jog2,jog ;
     double x,y;
	   
             if(agent->world().ball().pos().x > 43.0)
	     {
	         if(jogada_direita)
		  {
		      //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		      x = agent->world().ball().pos().x - VALOR_X; // 5 metros a frente
		      y = agent->world().ball().pos().y + 3*VALOR_Y; // 2 metros de lado  
		  }
		  else
		  {
		    //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		      x = agent->world().ball().pos().x - VALOR_X; // 5 metros a frente
		      y = agent->world().ball().pos().y + 3*VALOR_Y*(-1); // 2 metros de lado  
		  }
	     }
             else
	     {       
		  if(jogada_direita)
		  {
		      //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		      x = agent->world().ball().pos().x + VALOR_X; // 5 metros a frente
		      y = agent->world().ball().pos().y + 3*VALOR_Y; // 2 metros de lado  
		  }
		  else
		  {
		    //jog é a posição para onde o aliado deve ir para começar a jogada coletiva
		      x = agent->world().ball().pos().x + VALOR_X; // 5 metros a frente
		      y = agent->world().ball().pos().y + 3*VALOR_Y*(-1); // 2 metros de lado  
		  }
	     }
	     
	     
	     if(x<=agent->world().offsideLineX())
                  jog.x = (x>=52)?52:x; 
		else
		  jog.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog.y = (y>=34)?34:y;  
		else
		  jog.y = -34;
        x = y = 0;    
        
    ponto_desejado1 = jog;
	   
             if(agent->world().ball().pos().x > 43.0)
	     {
	        if(jogada_direita)
		{
		    x = agent->world().ball().pos().x - VALOR_X - 6;
		    y = agent->world().ball().pos().y -( VALOR_Y + 3);
		}
		else
		{
		  x = agent->world().ball().pos().x - (VALOR_X - 6);
		  y = agent->world().ball().pos().y + (VALOR_Y + 3);
		}
	     }
             else{
	       
		if(jogada_direita)
		{
		    x = agent->world().ball().pos().x + VALOR_X + 6;
		    y = agent->world().ball().pos().y -( VALOR_Y + 3);
		}
		else
		{
		  x = agent->world().ball().pos().x + (VALOR_X + 6);
		  y = agent->world().ball().pos().y + (VALOR_Y + 3);
		}
	       }
	       
	       
	   if(x<=agent->world().offsideLineX())
                  jog2.x = (x>=52)?52:x; 
		else
		  jog2.x = agent->world().offsideLineX();
	
		if(y>-34)
		  jog2.y = (y>=34)?34:y;  
		else
		  jog2.y = -34;
       
    ponto_desejado2 = jog2;		
        
    //double dist_from_ball = 100.0;
    double dist_from_point1 = 100.0, dist_from_point2 = 100.0;
     
    //procura o  agente mais próximo do ponto de destino ... (que não seja ele próprio, o goleiro
    const AbstractPlayerCont::const_iterator t_end = agent->world().allTeammates().end();
    for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	  //  if ( (*t)->unum() == nearest_teammate->unum() ) continue;
	    
	   if( (*t)->pos().dist(jog) < dist_from_point1)
	   {
	    dist_from_point1 = (*t)->pos().dist(jog);
	    num1 = (*t)->unum();
	   }	     
	  }
	  
	  //procura o agente mais próximo do segundo ponto de destino, que não seja ele próprio, o agente anterior ou o goleiro...
	   for ( AbstractPlayerCont::const_iterator t = agent->world().allTeammates().begin();
		t != t_end;
		++t )
	  {
	    if ( (*t)->goalie() ) continue;
	    if ( (*t)->isSelf() ) continue;
	    if( (*t)->unum() == num1 ) continue;
	    
	   if( (*t)->pos().dist(jog2) < dist_from_point2)
	   {
	    dist_from_point2 = (*t)->pos().dist(jog2);
	    num2 = (*t)->unum();
	   }
	     
	  }
	 
	  // garantindo que os números dos jogadores enviados sejam diferentes de NULL
	 if( (num1 == -1) || (num2==-1) )
	 {
	    const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false );
	   if (num1 ==-1)
	     num1 = nearest_teammate->unum();
	   else if(num2==-1 && num1 != nearest_teammate->unum())
	     num2 = nearest_teammate->unum();
	   
	  if (num1==-1)
	     for( int i=11; i>=2;i--)
	     {	       
	       if (i == wm.self().unum() ) continue;
	       
	       num1 = i; break;
	     }
	   
	   if (num2==-1)
	     for( int i=11; i>=2;i--)
	     {
	       if (i == num1) continue;
	       if (i == wm.self().unum() ) continue;
	       
	       num2 = i; break;
	     }
	     
	 }
	 
     // quantos ciclos o aliado vai levar pra chegar ao ponto que pedi para ele ir ?
     int ciclo_aliado = wm.teammate(num1)->playerTypePtr()->cyclesToReachDistance
      ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) );
      
    if((ciclo_aliado > 2) || ( wm.teammate(num1)->pos().dist( ponto_desejado1 ) > 2.0) )//se o aliado que vai receber a bola estah longe ainda...
    {
      //espera o jogador que vai participar da jogada chegar ao ponto... enquanto isso envi outra mensagem para mantê-lo guiado !
    
	  rcsc::jogadas jogada_atual = (jogada_direita)?(rcsc::assistencia_chute_dir):(rcsc::assistencia_chute_esq);
    
	 agent->addSayMessage( new JogadaMessage
		  (jogada_atual,ponto_desejado1, ponto_desejado2, num1, num2) ); 	
	  Bhv_GoToStaticBall( 0.0 ).execute( agent );// Body_ClearBall().execute( agent );
          agent->setNeckAction( new Neck_ScanField() );
	//  agent->setNeckAction( new Neck_TurnToBallOrScan() );
	  /*
	   * std:: cout << "\nA bola estah no ponto \nx = "<< agent->world().ball().pos().x
		             << "\ny = " << agent->world().ball().pos().y
		             << "\nmandei agent "<< num1 
		             <<", ir para ponto x = (" << ponto_desejado1.x
		             <<") e y = (" << ponto_desejado1.y << ")\n";
	 
	   */ 
	  return;
	
	//envia outra mensagem, se não, cobra a falta
      }else
      {
      
	num_jog = wm.audioMemory().jogada().front().j1_;
      
    // std::cout << "\nchutei para o player = " << num_jog  << "\n\n";
    
   // const PlayerObject * nearest_teammate = wm.getTeammateNearestToSelf( 10, false ); // without goalie
        if (  (wm.teammate(num_jog)->distFromSelf()< 20.0
	      &&  wm.teammate(num_jog)->pos().x > -30)
                  ||(  wm.teammate(num_jog)->distFromSelf() < 10.0 ) )
        {
            Vector2D target_point =   wm.teammate(num_jog)->inertiaFinalPoint(); //nearest_teammate->inertiaFinalPoint();
            target_point.x += 0.5;

            double ball_move_dist = wm.ball().pos().dist( target_point );
            int ball_reach_step
                = static_cast< int >( std::ceil( calc_length_geom_series( max_ball_speed,
                                                                          ball_move_dist,
                                                                          ServerParam::i().ballDecay() ) ) );
            double ball_speed = 0.0;
            if ( ball_reach_step > 3 )
            {
                ball_speed = calc_first_term_geom_series( ball_move_dist,
                                                          ServerParam::i().ballDecay(),
                                                          ball_reach_step );
            }
            else
            {
                ball_speed = calc_first_term_geom_series_last( 1.4,
                                                               ball_move_dist,
                                                               ServerParam::i().ballDecay() );
                ball_reach_step
                    = static_cast< int >( std::ceil( calc_length_geom_series( ball_speed,
                                                                              ball_move_dist,
                                                                              ServerParam::i().ballDecay() ) ) );
            }

            ball_speed = std::min( ball_speed, max_ball_speed );

            agent->debugClient().addMessage( "FreeKick:ForcePass%.3f", ball_speed );
            agent->debugClient().setTarget( target_point );
            dlog.addText( Logger::TEAM,
                          __FILE__":  force pass. target=(%.1f %.1f) speed=%.2f reach_step=%d",
                          target_point.x, target_point.y,
                          ball_speed, ball_reach_step );

            Body_KickOneStep( target_point, ball_speed*0.8 ).execute( agent );
	    agent->setNeckAction( new Neck_ScanField() );
    
    
    num_jog = 0;
    ponto_desejado1.x =0;
    ponto_desejado1.y =0;
	}
    return;
   
    }
}

     
     
//======================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//======================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//======================================================================================
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     
     

 /*=============================================================================================
 Logo abaixo estão descritos os métodos do sistema nebuloso que selecionará as jogadas coletivas
 a serem utilizadas para cada situação de jogo.
 =============================================================================================*/

/*! Chamada ao método difuso de seleção de jogada coletiva */ 
int Bhv_SetPlayFreeKick::tomada_decisao(rcsc::PlayerAgent *agent)
{
  
   // preencher com  dados colhidos no modelo de mundo, sendo respectivamente:
  // distancia ao gol adversário, posição em campo, ângulo ideal...
    DecidirJogada dec;
    double JogadaEscolhida;
    double hipotenusa = 9.0, distancia=0.0, alfa;

    const WorldModel & wm = agent->world();
    std::vector <Vector2D> oponentes;
    std::vector <double>angulos;
 
    std::vector <Vector2D> vetores;
    std::vector <double> angVetores;

    double melhorAngulo;
    Vector2D melhorPonto;
    double menorDist;
    

    //captura os oponentes a 15m, o vetor que eles fazem com a bola e o ângulo com relação a um vetor que tem o mesmo y da bola e o x um pouco adiantado
    for ( AbstractPlayerCont::const_iterator it = wm.allOpponents().begin();
              it != wm.allOpponents().end();
              ++it )
	 {
	   if(!(*it)->pos().isValid())
	       continue;
	   
       // captura a posição de todos os oponentes que estiverem a 15m de distância da bola
       // e que tiverem o x>=que o x da bola
	  if( wm.ball().pos().dist((*it)->pos()) <= 15.0 )
	  {
	    Vector2D eixoY, jogador;
	     // vetor que sai da bola e vai atá a borda da circunferencia de raio 9m
	    eixoY.x = wm.ball().pos().x + 9.0;// o------>x
	    eixoY.y =0.0; 
	    	    
	    oponentes.push_back((*it)->pos()); 
	    	    
	    jogador = (*it)->pos() - wm.ball().pos(); // jogador tem o vetor que sai da bola e vai para o jogador atual
	    vetores.push_back( jogador );
	    
	    angulos.push_back(
	    ((*it)->pos().y >= wm.ball().pos().y)?
	    ((
	    acos( ((jogador.x*eixoY.x + jogador.y*eixoY.y)/
	    (sqrt( pow(jogador.x,2) + pow(jogador.y,2)) * sqrt(pow(eixoY.x,2) + pow(eixoY.y,2))) ) )*180/PI)):
	    (acos( ((jogador.x*eixoY.x + jogador.y*eixoY.y)/
	    (sqrt( pow(jogador.x,2) + pow(jogador.y,2)) * sqrt(pow(eixoY.x,2) + pow(eixoY.y,2))) ) )*180/PI)*(-1)
	    );
	  }
	  
	}
    
    // se algum dos vetores estiver vazio ou se o tamnaho deles for diferente
    if(oponentes.size() == 0 || angulos.size()==0 || angulos.size() != oponentes.size() )
    { 
      return -1; //erro detectado
    }
      
    int i,j;
    Vector2D aux;
    double ang;
    
    // ordena os vetores em ordem decrescente de acordo com o ângulo
    for(i=0; i< oponentes.size(); i++)
   {
            
     for(j=0; j< oponentes.size(); j++)
   {   
       
       if( angulos[i] > angulos[j] )
       {                            
         // ordena oponentes            
         aux = oponentes[i];
         oponentes[i] = oponentes[j];
         oponentes[j] = aux;  
         
         //ordena angulos
         ang = angulos[i];
         angulos[i] = angulos[j];
         angulos[j] = ang;
         
         //ordena vetores
         aux = vetores[i];
         vetores[i] = vetores[j];
         vetores[j] = aux;
         
       }
       
    }
   
   }
    
   
   //calcula o angulo entre cada um dos vetores e o angulo que o oponente mais próximo faz dos extemos -90,90
   
   //vetor para cima da bola
    aux.x=0.0;
    aux.y = wm.ball().pos().y + 2.0 ;
    
    
    
    angVetores.push_back(acos( ((aux.x*vetores[0].x + aux.y*vetores[0].y)/
    (sqrt( pow(aux.x,2) + pow(aux.y,2)) * sqrt(pow(vetores[0].x,2) + pow(vetores[0].y,2))) ) )*180/PI);
   
   for(i=0;i<oponentes.size()-1;i++)
   {
   
     angVetores.push_back( acos( ((vetores[i].x*vetores[i+1].x + vetores[i].y*vetores[i+1].y)/
    (sqrt( pow(vetores[i].x,2) + pow(vetores[i].y,2)) * sqrt(pow(vetores[i+1].x,2) + pow(vetores[i+1].y,2))) ) )*180/PI  );
     
   }
    
    //vetor para baixo da bola...
    aux.x=0.0;
    aux.y= wm.ball().pos().y-2.0;
    
   angVetores.push_back( acos( ((vetores[i].x*aux.x + vetores[i].y*aux.y)/
    (sqrt( pow(vetores[i].x,2) + pow(vetores[i].y,2)) * sqrt(pow(aux.x,2) + pow(aux.y,2))) ) )*180/PI   );
   
   
   
    if( wm.ball().pos().isValid() &&  //  ----   1  ------
    wm.ball().pos().absY() <= 15.0  // se tiver entre -15 e 15 em y ( na região central do campo )
    && wm.ball().pos().x <= 43.0  // estah a mais de 9 metros do gol      
    )
    {
   
   // encontrar o melhor entre os ângulos ordenados ( o ponto em que o adversário mais próximo seja o mais distante )
   
   double ca, co, x, y, x1, y1, d, md;
   
   //considere um ponto como melhor inicialmente, para critérios de comparação
    melhorPonto.x =  wm.ball().pos().x + hipotenusa;
    melhorPonto.y = wm.ball().pos().y;
    
    menorDist = 100.0;
    alfa = 0.0;

    for(i=0;i<oponentes.size();i++)
    {
      distancia = sqrt(
      pow((oponentes[i].x-melhorPonto.x),2) +
      pow((oponentes[i].y-melhorPonto.y),2) );

      if(distancia < menorDist)
      menorDist = distancia;
    }
   
    //calcula a posição de um ponto que está a 9m de distancia que esteja mais longe de oponentes
  for(i=0;i<=angulos.size();i++)
  {
    if (i == 0)
      alfa = 90.0 - (angVetores[i]/2);
    else
      alfa = angulos[i-1] - (angVetores[i]/2);
    
    //Tratar casos de alfa = 90 e alfa = 0

    ca = (cos(alfa*PI/180))*hipotenusa;
    x = wm.ball().pos().x + ca;

    co = (sin(alfa*PI/180))*hipotenusa;
    y = wm.ball().pos().y + co;
    
    // coordenada (x,y) é um ponto que está a nove metros e passa pelo meio (do ângulo) dos dois vetores analisados

   md=100.0;
   //encontrar a distancia do oponente mais proximo a esse ponto
    for(j=0;j<oponentes.size();j++)
  {

    d = sqrt(
    pow((oponentes[j].x-x),2) +
    pow((oponentes[j].y-y),2) );

    if(d < md)
     md = d;
  }

  // o oponente que está mais próximo desse ponto, está a mais longe desse ponto do que o oponente que está mais próximo do ponto analisado anteriormente,
  //ou seja, dentre os oponentes que estão mais perto de cada ponto quero o que estiver mais longe
  if( md > menorDist )
    {
      melhorAngulo = alfa;
      melhorPonto.x = x;
      melhorPonto.y = y;
      menorDist = md;

      x1 = oponentes[i-1].x;
      y1 = oponentes[i-1].y;

    }


  }
}  //  ----   2  ------
else if( wm.ball().pos().isValid() && wm.ball().pos().x >= 43.0 ) // estah a menos de 9 metros da linha de fundo     
    
{
  // nao considerar ângulos frontal e direito
  
   if( wm.ball().pos().y >= 0.0 )
  {
    // considere somente o angulo direito
    
   // encontrar o melhor entre os ângulos ordenados ( o ponto em que o adversário mais próximo seja o mais distante )
    double ca, co, x, y, x1, y1, d, md;
   
   //considere um ponto como melhor inicialmente, para critérios de comparação
    melhorPonto.x =  wm.ball().pos().x - hipotenusa;
    melhorPonto.y = wm.ball().pos().y;
    
    menorDist = 100.0;
    alfa = 90.0;

    for(i=0;i<oponentes.size();i++)
    {
      distancia = sqrt(
      pow((oponentes[i].x-melhorPonto.x),2) +
      pow((oponentes[i].y-melhorPonto.y),2) );

      if(distancia < menorDist)
      menorDist = distancia;
    }
   
    //calcula a posição de um ponto que está a 9m de distancia que esteja mais longe de oponentes
  for(i=0;i<=angulos.size();i++)
  {
    if (i == 0)
      alfa = 90.0 - (angVetores[i]/2);
    else
      alfa = angulos[i-1] - (angVetores[i]/2);
    
    //Tratar casos de alfa = 90 e alfa = 0

    ca = (cos(alfa*PI/180))*hipotenusa;
    x = wm.ball().pos().x + ca;

    co = (sin(alfa*PI/180))*hipotenusa;
    y = wm.ball().pos().y + co;
    
    // coordenada (x,y) é um ponto que está a nove metros e passa pelo meio (do ângulo) dos dois vetores analisados
    
    //////////////////////////////////////////////////
    
    if( (x >= 52.0) || (y>= 34.0) || alfa < 0.0) // está fora do campo
      continue;
    
    //////////////////////////////////////////////////
    
   md=100.0;
   //encontrar a distancia do oponente mais proximo a esse ponto
    for(j=0;j<oponentes.size();j++)
  {

    d = sqrt(
    pow((oponentes[j].x-x),2) +
    pow((oponentes[j].y-y),2) );

    if(d < md)
     md = d;
  }

  // o oponente que está mais próximo desse ponto, está a mais longe desse ponto do que o oponente que está mais próximo do ponto analisado anteriormente, ou seja, dentre os oponentes que estão mais perto de cada ponto quero o que estiver mais longe
  if( md > menorDist )
    {
      melhorAngulo = alfa;
      melhorPonto.x = x;
      melhorPonto.y = y;
      menorDist = md;

      x1 = oponentes[i-1].x;
      y1 = oponentes[i-1].y;

    }


  }
    
  }
  else
  {
    // considere somente o angulo esquerdo
    
       // encontrar o melhor entre os ângulos ordenados ( o ponto em que o adversário mais próximo seja o mais distante )
    double ca, co, x, y, x1, y1, d, md;
   
   //considere um ponto como melhor inicialmente, para critérios de comparação
    melhorPonto.x =  wm.ball().pos().x - hipotenusa;
    melhorPonto.y = wm.ball().pos().y;
    
    menorDist = 100.0;
    alfa = -90.0;

    for(i=0;i<oponentes.size();i++)
    {
      distancia = sqrt(
      pow((oponentes[i].x-melhorPonto.x),2) +
      pow((oponentes[i].y-melhorPonto.y),2) );

      if(distancia < menorDist)
      menorDist = distancia;
    }
   
    //calcula a posição de um ponto que está a 9m de distancia que esteja mais longe de oponentes
  for(i=0;i<=angulos.size();i++)
  {
    if (i == 0)
      alfa = 90.0 - (angVetores[i]/2);
    else
      alfa = angulos[i-1] - (angVetores[i]/2);
    
    //Tratar casos de alfa = 90 e alfa = 0

    ca = (cos(alfa*PI/180))*hipotenusa;
    x = wm.ball().pos().x + ca;

    co = (sin(alfa*PI/180))*hipotenusa;
    y = wm.ball().pos().y + co;
    
    // coordenada (x,y) é um ponto que está a nove metros e passa pelo meio (do ângulo) dos dois vetores analisados
    
    //////////////////////////////////////////////////
    
    if( (x >= 52.0) || (y<= -34.0) || alfa > 0.0) // está fora do campo
      continue;
    
    //////////////////////////////////////////////////
    
   md=100.0;
   //encontrar a distancia do oponente mais proximo a esse ponto
    for(j=0;j<oponentes.size();j++)
  {

    d = sqrt(
    pow((oponentes[j].x-x),2) +
    pow((oponentes[j].y-y),2) );

    if(d < md)
     md = d;
  }

  // o oponente que está mais próximo desse ponto, está a mais longe desse ponto do que o oponente que está mais próximo do ponto analisado anteriormente, ou seja, dentre os oponentes que estão mais perto de cada ponto quero o que estiver mais longe
  if( md > menorDist )
    {
      melhorAngulo = alfa;
      melhorPonto.x = x;
      melhorPonto.y = y;
      menorDist = md;

      x1 = oponentes[i-1].x;
      y1 = oponentes[i-1].y;

    }


  }

    
  }
  
}  //  ----   3  ------
else if( wm.ball().pos().isValid() &&
        wm.ball().pos().y <= -15.0 && // se tiver bem do lado esquerdo
       wm.ball().pos().x < 43.0  // estah a mais de 9 metros do gol      
    )
{
   // desconsiderar avaliações de ângulos pela esquerda
   
         // encontrar o melhor entre os ângulos ordenados ( o ponto em que o adversário mais próximo seja o mais distante )
    double ca, co, x, y, x1, y1, d, md;
   
   //considere um ponto como melhor inicialmente, para critérios de comparação
    melhorPonto.x =  wm.ball().pos().x + hipotenusa;
    melhorPonto.y = wm.ball().pos().y;
    
    menorDist = 100.0;
    alfa = -90.0;

    for(i=0;i<oponentes.size();i++)
    {
      distancia = sqrt(
      pow((oponentes[i].x-melhorPonto.x),2) +
      pow((oponentes[i].y-melhorPonto.y),2) );

      if(distancia < menorDist)
      menorDist = distancia;
    }
   
    //calcula a posição de um ponto que está a 9m de distancia que esteja mais longe de oponentes
  for(i=0;i<=angulos.size();i++)
  {
    if (i == 0)
      alfa = 90.0 - (angVetores[i]/2);
    else
      alfa = angulos[i-1] - (angVetores[i]/2);
    
    //Tratar casos de alfa = 90 e alfa = 0

    ca = (cos(alfa*PI/180))*hipotenusa;
    x = wm.ball().pos().x + ca;

    co = (sin(alfa*PI/180))*hipotenusa;
    y = wm.ball().pos().y + co;
    
    // coordenada (x,y) é um ponto que está a nove metros e passa pelo meio (do ângulo) dos dois vetores analisados
    
    //////////////////////////////////////////////////
    
    if( (x >= 52.0) || (y<= -34.0) || alfa > 0.0) // está fora do campo
      continue;
    
    //////////////////////////////////////////////////
    
   md=100.0;
   //encontrar a distancia do oponente mais proximo a esse ponto
    for(j=0;j<oponentes.size();j++)
  {

    d = sqrt(
    pow((oponentes[j].x-x),2) +
    pow((oponentes[j].y-y),2) );

    if(d < md)
     md = d;
  }

  // o oponente que está mais próximo desse ponto, está a mais longe desse ponto do que o oponente que está mais próximo do ponto analisado anteriormente, ou seja, dentre os oponentes que estão mais perto de cada ponto quero o que estiver mais longe
  if( md > menorDist )
    {
      melhorAngulo = alfa;
      melhorPonto.x = x;
      melhorPonto.y = y;
      menorDist = md;

      x1 = oponentes[i-1].x;
      y1 = oponentes[i-1].y;

    }


  }

    
  
   
}  //  ----   4  ------
else if( wm.ball().pos().isValid() &&
        wm.ball().pos().y >= 15 && // se tiver mais do lado direito
       wm.ball().pos().x < 43  // estah a mais de 9 metros da linha de fundo      
    )  
{
  //desconsiderar avaliações de ângulos pela direita

// encontrar o melhor entre os ângulos ordenados ( o ponto em que o adversário mais próximo seja o mais distante )
    double ca, co, x, y, x1, y1, d, md;
   
   //considere um ponto como melhor inicialmente, para critérios de comparação
    melhorPonto.x =  wm.ball().pos().x + hipotenusa;
    melhorPonto.y = wm.ball().pos().y;
    
    menorDist = 100.0;
    alfa = 90.0;

    for(i=0;i<oponentes.size();i++)
    {
      distancia = sqrt(
      pow((oponentes[i].x-melhorPonto.x),2) +
      pow((oponentes[i].y-melhorPonto.y),2) );

      if(distancia < menorDist)
      menorDist = distancia;
    }
   
    //calcula a posição de um ponto que está a 9m de distancia que esteja mais longe de oponentes
  for(i=0;i<=angulos.size();i++)
  {
    if (i == 0)
      alfa = 90.0 - (angVetores[i]/2);
    else
      alfa = angulos[i-1] - (angVetores[i]/2);
    
    //Tratar casos de alfa = 90 e alfa = 0

    ca = (cos(alfa*PI/180))*hipotenusa;
    x = wm.ball().pos().x + ca;

    co = (sin(alfa*PI/180))*hipotenusa;
    y = wm.ball().pos().y + co;
    
    // coordenada (x,y) é um ponto que está a nove metros e passa pelo meio (do ângulo) dos dois vetores analisados
    
    //////////////////////////////////////////////////
    
    if( (x >= 52.0) || (y>= 34.0) || alfa < 0.0) // está fora do campo
      continue;
    
    //////////////////////////////////////////////////
    
   md=100.0;
   //encontrar a distancia do oponente mais proximo a esse ponto
    for(j=0;j<oponentes.size();j++)
  {

    d = sqrt(
    pow((oponentes[j].x-x),2) +
    pow((oponentes[j].y-y),2) );

    if(d < md)
     md = d;
  }

  // o oponente que está mais próximo desse ponto, está a mais longe desse ponto do que o oponente que está mais próximo do ponto analisado anteriormente, ou seja, dentre os oponentes que estão mais perto de cada ponto quero o que estiver mais longe
  if( md > menorDist )
    {
      melhorAngulo = alfa;
      melhorPonto.x = x;
      melhorPonto.y = y;
      menorDist = md;

      x1 = oponentes[i-1].x;
      y1 = oponentes[i-1].y;

    }


  }

}
else
{
  //erro detectado, situação anômala
   return -1;
  
}
  

  if (melhorPonto.y < wm.ball().pos().y) alfa = alfa*(-1);
   
  
 
  //preencher os valores da inferencia...  
  
  if(melhorAngulo>90.0)
    melhorAngulo=90.0;
  else if (melhorAngulo < -90.0)
    melhorAngulo = -90.0;
    
  double num1, num2;
  Vector2D gol;
  gol.x = 52.5;
  gol.y = wm.ball().pos().y;
  
  // ditancia até a linha de fundo
  if(wm.ball().pos().dist(gol) > 80.0 )
     num1 = 80.0;
  else
    num1 = wm.ball().pos().dist(gol);
  
  // posição em relação ao eixo y
  if( wm.ball().pos().y > 25 )
     num2 = 25;
  else if(wm.ball().pos().y < -25)
    num2 = -25;
  else
    num2 = wm.ball().pos().y;
  
/*+++***+++***+++***+++***+++***+++***+++***+++***+++***+++***+++*/ 
  
    dec.inference(num1,num2,melhorAngulo, &JogadaEscolhida);
    // de acordo com o valor da saída, selecionar jogada coletiva!
  
/*+++***+++***+++***+++***+++***+++***+++***+++***+++***+++***+++*/    
    
    // "pontoInicial" é o ponto onde a jogada irá iniciar... 
    pontoInicial = melhorPonto;    

    if(JogadaEscolhida <= 5) // inversão pela esquerda
    {
     return rcsc::inversao_esq;
      
    } else if(JogadaEscolhida < 15) // lateral com lançamento pela esquerda
    {
      return rcsc::lateral_lanc_esq;
      
    }else if(JogadaEscolhida <= 25) // assistência para chute pela esquerda
    {
      return rcsc::assistencia_chute_esq;
      
    }else if(JogadaEscolhida < 35) // tabela pela esquerda
    {
      return rcsc::tabela_esq;
      
    }else if(JogadaEscolhida <= 45) // bola pelo meio com abertura pela lateral esquerda
    {
      return rcsc::meio_aberto_lateral_esq;
      
    }else if(JogadaEscolhida < 55) // bola pelo meio com abertura pela lateral direita 
    {
      return rcsc::meio_aberto_lateral_dir;
      
    }else if(JogadaEscolhida <= 65) // tabela pela direita
    {
      return rcsc::tabela_dir;
      
    }else if(JogadaEscolhida < 75) // assitência para chute pela direita
    {
      return rcsc::assistencia_chute_dir;
      
    }else if(JogadaEscolhida <= 85) // lateral com lançamento pela direita
    {
      return rcsc::lateral_lanc_dir;
      
    }else if(JogadaEscolhida > 85) // inversão pela direita
    {
      return rcsc::inversao_dir;
      
    }
    
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
     //       Membership function de entrada      //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

ParamMembershipFunction::ParamMembershipFunction(double min, double max,
double step) {
 this->min = min;
 this->max = max;
 this->step = step;
}

double ParamMembershipFunction::center() { return 0; }

double ParamMembershipFunction::basis() { return 0; }

double ParamMembershipFunction::compute_smeq(double x) {
 double degree=0, mu;
 for(double y=max; y>=x ; y-=step) if((mu = compute_eq(y))>degree) degree=mu;
 return degree;
}

double ParamMembershipFunction::compute_greq(double x) {
 double degree=0, mu;
 for(double y=min; y<=x ; y+=step) if((mu = compute_eq(y))>degree) degree=mu;
 return degree;
}

double ParamMembershipFunction::isEqual(MembershipFunction &mf) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return compute_eq( ((FuzzySingleton &) mf).getValue()); }
 double mu1,mu2,minmu,degree=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = compute_eq(omf.conc[i]->param(0));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = compute_eq(x);
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isGreaterOrEqual(MembershipFunction &mf) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return compute_greq( ((FuzzySingleton &) mf).getValue()); }
 double mu1,mu2,minmu,degree=0,greq=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = compute_greq(omf.conc[i]->param(0));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = compute_eq(x);
   if( mu2>greq ) greq = mu2;
   if( mu1<greq ) minmu = mu1; else minmu = greq;
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isSmallerOrEqual(MembershipFunction &mf) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return compute_smeq( ((FuzzySingleton &) mf).getValue()); }
 double mu1,mu2,minmu,degree=0,smeq=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = compute_smeq(omf.conc[i]->param(0));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=max; x>=min; x-=step){
   mu1 = mf.compute(x);
   mu2 = compute_eq(x);
   if( mu2>smeq ) smeq = mu2;
   if( mu1<smeq ) minmu = mu1; else minmu = smeq;
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isGreater(MembershipFunction &mf,
Operatorset &op) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return op.not1(compute_smeq( ((FuzzySingleton &) mf).getValue())); }
 double mu1,mu2,minmu,gr,degree=0,smeq=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = op.not1(compute_smeq(omf.conc[i]->param(0)));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=max; x>=min; x-=step){
   mu1 = mf.compute(x);
   mu2 = compute_eq(x);
   if( mu2>smeq ) smeq = mu2;
   gr = op.not1(smeq);
   minmu = ( mu1<gr ? mu1 : gr);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isSmaller(MembershipFunction &mf,
Operatorset &op) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return op.not1(compute_greq( ((FuzzySingleton &) mf).getValue())); }
 double mu1,mu2,minmu,sm,degree=0,greq=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = op.not1(compute_greq(omf.conc[i]->param(0)));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = compute_eq(x);
   if( mu2>greq ) greq = mu2;
   sm = op.not1(greq);
   minmu = ( mu1<sm ? mu1 : sm);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isNotEqual(MembershipFunction &mf,
Operatorset &op) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return op.not1(compute_eq( ((FuzzySingleton &) mf).getValue())); }
 double mu1,mu2,minmu,degree=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = op.not1(compute_eq(omf.conc[i]->param(0)));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = op.not1(compute_eq(x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isApproxEqual(MembershipFunction &mf,
Operatorset &op) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return op.moreorless(compute_eq( ((FuzzySingleton &) mf).getValue())); }
 double mu1,mu2,minmu,degree=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = op.moreorless(compute_eq(omf.conc[i]->param(0)));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = op.moreorless(compute_eq(x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isVeryEqual(MembershipFunction &mf,
Operatorset &op) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return op.very(compute_eq( ((FuzzySingleton &) mf).getValue())); }
 double mu1,mu2,minmu,degree=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = op.very(compute_eq(omf.conc[i]->param(0)));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = op.very(compute_eq(x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

double ParamMembershipFunction::isSlightlyEqual(MembershipFunction &mf,
Operatorset &op) {
 if( mf.getType() == MembershipFunction::CRISP )
  { return op.slightly(compute_eq( ((FuzzySingleton &) mf).getValue())); }
 double mu1,mu2,minmu,degree=0;
 if( mf.getType() == MembershipFunction::INNER &&
     ((OutputMembershipFunction &) mf).isDiscrete() ){
  OutputMembershipFunction &omf = (OutputMembershipFunction &) mf;
  for(int i=0; i<omf.length; i++){
   mu1 = omf.conc[i]->degree();
   mu2 = op.slightly(compute_eq(omf.conc[i]->param(0)));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(double x=min; x<=max; x+=step){
   mu1 = mf.compute(x);
   mu2 = op.slightly(compute_eq(x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

int ParamMembershipFunction::isXflSingleton() {
 int size = strlen(name);
 if(size < 14) return 0;
 if( !strcmp(&name[size-14],"_xfl_singleton") ) return 1;
 return 0;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//       Membership function da variável de saída     //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

OutputMembershipFunction::OutputMembershipFunction(Operatorset *op,int length,int inputlength,double *input){
 this->op = op;
 this->length = length;
 this->inputlength = inputlength;
 this->input = input;
 this->conc = new RuleConclusion*[length];
}
double OutputMembershipFunction::compute(double x) {
 double dom = op->imp(conc[0]->degree(),conc[0]->compute(x));
 for(int i=1; i<length; i++)
   dom = op->also(dom,op->imp(conc[i]->degree(),conc[i]->compute(x)));
 return dom;
}
int OutputMembershipFunction::isDiscrete() {
 for(int i=0; i<length; i++) if(!conc[i]->isDiscrete()) return 0;
 return 1;
}

#ifndef _DecidirJogada_INFERENCE_ENGINE_
#define _DecidirJogada_INFERENCE_ENGINE_

//+++++++++++++++++++++++++++++++++++++//
//  MembershipFunction MF_DecidirJogada_xfl_triangle  //
//+++++++++++++++++++++++++++++++++++++//

MF_DecidirJogada_xfl_triangle::MF_DecidirJogada_xfl_triangle(double min,double max,double step,double *param, int length) :
ParamMembershipFunction(min,max,step) {
 this->name = "MF_DecidirJogada_xfl_triangle";
 this->a = param[0];
 this->b = param[1];
 this->c = param[2];
}

MF_DecidirJogada_xfl_triangle * MF_DecidirJogada_xfl_triangle::dup() {
 double param[3] = {a,b,c};
 return new MF_DecidirJogada_xfl_triangle(min,max,step,param,3);
}

double MF_DecidirJogada_xfl_triangle::param(int _i) {
 switch(_i) {
  case 0: return a;
  case 1: return b;
  case 2: return c;
  default: return 0;
 }
}

double MF_DecidirJogada_xfl_triangle::compute_eq(double x) {
  return (a<x && x<=b? (x-a)/(b-a) : (b<x && x<c? (c-x)/(c-b) : 0)); 
}

double MF_DecidirJogada_xfl_triangle::compute_greq(double x) {
  return (x<a? 0 : (x>b? 1 : (x-a)/(b-a) )); 
}

double MF_DecidirJogada_xfl_triangle::compute_smeq(double x) {
  return (x<b? 1 : (x>c? 0 : (c-x)/(c-b) )); 
}

double MF_DecidirJogada_xfl_triangle::center() {
  return b; 
}

double MF_DecidirJogada_xfl_triangle::basis() {
  return (c-a); 
}

//+++++++++++++++++++++++++++++++++++++//
//  MembershipFunction MF_DecidirJogada_xfl_trapezoid  //
//+++++++++++++++++++++++++++++++++++++//

MF_DecidirJogada_xfl_trapezoid::MF_DecidirJogada_xfl_trapezoid(double min,double max,double step,double *param, int length) :
ParamMembershipFunction(min,max,step) {
 this->name = "MF_DecidirJogada_xfl_trapezoid";
 this->a = param[0];
 this->b = param[1];
 this->c = param[2];
 this->d = param[3];
}

MF_DecidirJogada_xfl_trapezoid * MF_DecidirJogada_xfl_trapezoid::dup() {
 double param[4] = {a,b,c,d};
 return new MF_DecidirJogada_xfl_trapezoid(min,max,step,param,4);
}

double MF_DecidirJogada_xfl_trapezoid::param(int _i) {
 switch(_i) {
  case 0: return a;
  case 1: return b;
  case 2: return c;
  case 3: return d;
  default: return 0;
 }
}

double MF_DecidirJogada_xfl_trapezoid::compute_eq(double x) {
  return (x<a || x>d? 0: (x<b? (x-a)/(b-a) : (x<c?1 : (d-x)/(d-c)))); 
}

double MF_DecidirJogada_xfl_trapezoid::compute_greq(double x) {
  return (x<a? 0 : (x>b? 1 : (x-a)/(b-a) )); 
}

double MF_DecidirJogada_xfl_trapezoid::compute_smeq(double x) {
  return (x<c? 1 : (x>d? 0 : (d-x)/(d-c) )); 
}

double MF_DecidirJogada_xfl_trapezoid::center() {
  return (b+c)/2; 
}

double MF_DecidirJogada_xfl_trapezoid::basis() {
  return (d-a); 
}

//+++++++++++++++++++++++++++++++++++++//
//  Operatorset OP_DecidirJogada_DefinicaoOperador //
//+++++++++++++++++++++++++++++++++++++//

double OP_DecidirJogada_DefinicaoOperador::and1(double a, double b) {
  return (a<b? a : b); 
}
double OP_DecidirJogada_DefinicaoOperador::or1(double a, double b) {
  return (a>b? a : b); 
}
double OP_DecidirJogada_DefinicaoOperador::also(double a, double b) {
  return (a>b? a : b); 
}
double OP_DecidirJogada_DefinicaoOperador::imp(double a, double b) {
  return (a<b? a : b); 
}
double OP_DecidirJogada_DefinicaoOperador::not1(double a) {
  return 1-a; 
}

double OP_DecidirJogada_DefinicaoOperador::very(double a) {
 double w = 2.0;
  return pow(a,w); 
}

double OP_DecidirJogada_DefinicaoOperador::moreorless(double a) {
 double w = 0.5;
  return pow(a,w); 
}

double OP_DecidirJogada_DefinicaoOperador::slightly(double a) {
  return 4*a*(1-a); 
}

double OP_DecidirJogada_DefinicaoOperador::defuz(OutputMembershipFunction &mf) {
 double min = mf.min();
 double max = mf.max();
 double step = mf.step();
   double num=0, denom=0;
   for(double x=min; x<=max; x+=step) {
    double m = mf.compute(x);
    num += x*m;
    denom += m;
   }
   if(denom==0) return (min+max)/2;
   return num/denom;
}

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_DistanciaGolAdv //
//+++++++++++++++++++++++++++++++++++++//

TP_DecidirJogada_DistanciaGolAdv::TP_DecidirJogada_DistanciaGolAdv() {
 min = 0.0;
 max = 80.0;
 step = 0.06672226855713094;
 double _p_MuitoPerto[3] = { -10.0,0.0,10.0 };
 double _p_Perto[3] = { 5.0,10.0,15.0 };
 double _p_Media[3] = { 10.0,15.0,25.0 };
 double _p_Longe[3] = { 15.0,25.0,50.0 };
 double _p_MuitoLonge[4] = { 25.0,50.0,80.0,93.33333333333333 };
 MuitoPerto = MF_DecidirJogada_xfl_triangle(min,max,step,_p_MuitoPerto,3);
 Perto = MF_DecidirJogada_xfl_triangle(min,max,step,_p_Perto,3);
 Media = MF_DecidirJogada_xfl_triangle(min,max,step,_p_Media,3);
 Longe = MF_DecidirJogada_xfl_triangle(min,max,step,_p_Longe,3);
 MuitoLonge = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_MuitoLonge,4);
}

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_PosicaoCampo //
//+++++++++++++++++++++++++++++++++++++//

TP_DecidirJogada_PosicaoCampo::TP_DecidirJogada_PosicaoCampo() {
 min = -25.0;
 max = 25.0;
 step = 0.020008003201280513;
 double _p_Esquerda[4] = { -37.5,-25.0,-12.5,0.0 };
 double _p_Meio[3] = { -12.5,0.0,12.5 };
 double _p_Direita[4] = { 0.0,12.5,25.0,37.5 };
 Esquerda = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_Esquerda,4);
 Meio = MF_DecidirJogada_xfl_triangle(min,max,step,_p_Meio,3);
 Direita = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_Direita,4);
}

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_MelhorAngulo //
//+++++++++++++++++++++++++++++++++++++//

TP_DecidirJogada_MelhorAngulo::TP_DecidirJogada_MelhorAngulo() {
 min = -90.0;
 max = 90.0;
 step = 0.1000555864369094;
 double _p_Esquerda[4] = { -100.0,-90.0,-65.0,-50.0 };
 double _p_MeioEsquerda[4] = { -65.0,-50.0,-25.0,-15.0 };
 double _p_Frontal[4] = { -25.0,-15.0,15.0,25.0 };
 double _p_MeioDireita[4] = { 15.0,25.0,50.0,65.0 };
 double _p_Direita[4] = { 50.0,65.0,90.0,100.0 };
 Esquerda = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_Esquerda,4);
 MeioEsquerda = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_MeioEsquerda,4);
 Frontal = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_Frontal,4);
 MeioDireita = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_MeioDireita,4);
 Direita = MF_DecidirJogada_xfl_trapezoid(min,max,step,_p_Direita,4);
}

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_Jogada //
//+++++++++++++++++++++++++++++++++++++//

TP_DecidirJogada_Jogada::TP_DecidirJogada_Jogada() {
 min = 0.0;
 max = 90.0;
 step = 0.04502251125562781;
 double _p_InversaoEsq[3] = { -10.0,0.0,10.0 };
 double _p_LateralLancEsq[3] = { 0.0,10.0,20.0 };
 double _p_AssistenciaChuteEsq[3] = { 10.0,20.0,30.0 };
 double _p_TabelaEsq[3] = { 20.0,30.0,40.0 };
 double _p_MeioAberLatEsq[3] = { 30.0,40.0,50.0 };
 double _p_MeioAberLatDir[3] = { 40.0,50.0,60.0 };
 double _p_TabelaDir[3] = { 50.0,60.0,70.0 };
 double _p_AssistenciaChuteDir[3] = { 60.0,70.0,80.0 };
 double _p_LateralLancDir[3] = { 70.0,80.0,90.0 };
 double _p_InversaoDir[3] = { 80.0,90.0,100.0 };
 InversaoEsq = MF_DecidirJogada_xfl_triangle(min,max,step,_p_InversaoEsq,3);
 LateralLancEsq = MF_DecidirJogada_xfl_triangle(min,max,step,_p_LateralLancEsq,3);
 AssistenciaChuteEsq = MF_DecidirJogada_xfl_triangle(min,max,step,_p_AssistenciaChuteEsq,3);
 TabelaEsq = MF_DecidirJogada_xfl_triangle(min,max,step,_p_TabelaEsq,3);
 MeioAberLatEsq = MF_DecidirJogada_xfl_triangle(min,max,step,_p_MeioAberLatEsq,3);
 MeioAberLatDir = MF_DecidirJogada_xfl_triangle(min,max,step,_p_MeioAberLatDir,3);
 TabelaDir = MF_DecidirJogada_xfl_triangle(min,max,step,_p_TabelaDir,3);
 AssistenciaChuteDir = MF_DecidirJogada_xfl_triangle(min,max,step,_p_AssistenciaChuteDir,3);
 LateralLancDir = MF_DecidirJogada_xfl_triangle(min,max,step,_p_LateralLancDir,3);
 InversaoDir = MF_DecidirJogada_xfl_triangle(min,max,step,_p_InversaoDir,3);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//  Base de Regras utilizada pelo sistema de inferência fuzzy //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//

void DecidirJogada::RL_ProcessoDecisao(MembershipFunction &PosX, MembershipFunction &PosY, MembershipFunction &AngIdeal, MembershipFunction ** _o_jogada) {
 OP_DecidirJogada_DefinicaoOperador _op;
 double _rl;
 int _i_jogada=0;
 double *_input = new double[3];
 _input[0] = PosX.getValue();
 _input[1] = PosY.getValue();
 _input[2] = AngIdeal.getValue();
 OutputMembershipFunction *jogada = new OutputMembershipFunction(new OP_DecidirJogada_DefinicaoOperador(),34,3,_input);
 TP_DecidirJogada_DistanciaGolAdv _t_PosX;
 TP_DecidirJogada_PosicaoCampo _t_PosY;
 TP_DecidirJogada_MelhorAngulo _t_AngIdeal;
 TP_DecidirJogada_Jogada _t_jogada;
 _rl = _op.and1(_t_PosX.MuitoPerto.isEqual(PosX),_t_PosY.Esquerda.isEqual(PosY));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoDir.dup()); _i_jogada++;
 _rl = _op.and1(_t_PosX.MuitoPerto.isEqual(PosX),_t_PosY.Direita.isEqual(PosY));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoPerto.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.AssistenciaChuteEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoPerto.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioDireita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.AssistenciaChuteDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Perto.isEqual(PosX),_op.very(_t_PosY.Esquerda.isEqual(PosY))),_t_AngIdeal.Direita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Perto.isEqual(PosX),_op.very(_t_PosY.Direita.isEqual(PosY))),_t_AngIdeal.Esquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Perto.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_op.or1(_t_AngIdeal.Direita.isEqual(AngIdeal),_op.very(_t_AngIdeal.MeioDireita.isEqual(AngIdeal))));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.AssistenciaChuteDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Perto.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_op.or1(_t_AngIdeal.Esquerda.isEqual(AngIdeal),_op.very(_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal))));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.AssistenciaChuteEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_t_PosY.Esquerda.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_op.very(_t_PosY.Direita.isEqual(PosY))),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_op.very(_t_PosY.Esquerda.isEqual(PosY))),_op.or1(_t_AngIdeal.Direita.isEqual(AngIdeal),_t_AngIdeal.MeioDireita.isEqual(AngIdeal)));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_op.very(_t_PosY.Direita.isEqual(PosY))),_op.or1(_t_AngIdeal.Esquerda.isEqual(AngIdeal),_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal)));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_op.very(_t_AngIdeal.Esquerda.isEqual(AngIdeal)));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.LateralLancEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_op.very(_t_AngIdeal.Direita.isEqual(AngIdeal)));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.LateralLancDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.AssistenciaChuteEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Media.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioDireita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.AssistenciaChuteDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Esquerda.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Direita.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Direita.isEqual(PosY)),_op.or1(_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal),_t_AngIdeal.Esquerda.isEqual(AngIdeal)));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Esquerda.isEqual(PosY)),_op.or1(_t_AngIdeal.MeioDireita.isEqual(AngIdeal),_t_AngIdeal.Direita.isEqual(AngIdeal)));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.TabelaEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioDireita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.TabelaDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.Esquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.LateralLancEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.Direita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.LateralLancDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.Longe.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.TabelaEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Meio.isEqual(PosY)),_t_AngIdeal.MeioDireita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.TabelaDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_op.or1(_t_PosY.Meio.isEqual(PosY),_t_PosY.Direita.isEqual(PosY))),_t_AngIdeal.Esquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_op.or1(_t_PosY.Meio.isEqual(PosY),_op.very(_t_PosY.Esquerda.isEqual(PosY)))),_t_AngIdeal.Direita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.InversaoDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Esquerda.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Direita.isEqual(PosY)),_t_AngIdeal.Frontal.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.MeioAberLatEsq.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Esquerda.isEqual(PosY)),_t_AngIdeal.MeioDireita.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.LateralLancDir.dup()); _i_jogada++;
 _rl = _op.and1(_op.and1(_t_PosX.MuitoLonge.isEqual(PosX),_t_PosY.Direita.isEqual(PosY)),_t_AngIdeal.MeioEsquerda.isEqual(AngIdeal));
 (*jogada).conc[_i_jogada] = new RuleConclusion(_rl, _t_jogada.LateralLancEsq.dup()); _i_jogada++;
 *_o_jogada = new FuzzySingleton( (*jogada).defuzzify());
 delete jogada;
 delete _input;
}
//+++++++++++++++++++++++++++++++++++++//
//      Motor de Inferência Fuzzy      //
//+++++++++++++++++++++++++++++++++++++//

double* DecidirJogada::crispInference(double *_input) {
 FuzzySingleton PosicaoX(_input[0]);
 FuzzySingleton PosicaoY(_input[1]);
 FuzzySingleton AnguloIdeal(_input[2]);
 MembershipFunction *JogadaSelecionada;
 RL_ProcessoDecisao(PosicaoX, PosicaoY, AnguloIdeal, &JogadaSelecionada);
 double *_output = new double[1];
 if(JogadaSelecionada->getType() == MembershipFunction::CRISP) _output[0] = ((FuzzySingleton *) JogadaSelecionada)->getValue();
 else _output[0] = ((OutputMembershipFunction *) JogadaSelecionada)->defuzzify();
 delete JogadaSelecionada;
 return _output;
}

double* DecidirJogada::crispInference(MembershipFunction* &_input) {
 MembershipFunction & PosicaoX = _input[0];
 MembershipFunction & PosicaoY = _input[1];
 MembershipFunction & AnguloIdeal = _input[2];
 MembershipFunction *JogadaSelecionada;
 RL_ProcessoDecisao(PosicaoX, PosicaoY, AnguloIdeal, &JogadaSelecionada);
 double *_output = new double[1];
 if(JogadaSelecionada->getType() == MembershipFunction::CRISP) _output[0] = ((FuzzySingleton *) JogadaSelecionada)->getValue();
 else _output[0] = ((OutputMembershipFunction *) JogadaSelecionada)->defuzzify();
 delete JogadaSelecionada;
 return _output;
}

MembershipFunction** DecidirJogada::fuzzyInference(double *_input) {
 FuzzySingleton PosicaoX(_input[0]);
 FuzzySingleton PosicaoY(_input[1]);
 FuzzySingleton AnguloIdeal(_input[2]);
 MembershipFunction *JogadaSelecionada;
 RL_ProcessoDecisao(PosicaoX, PosicaoY, AnguloIdeal, &JogadaSelecionada);
 MembershipFunction **_output = new MembershipFunction *[1];
 _output[0] = JogadaSelecionada;
 return _output;
}

MembershipFunction** DecidirJogada::fuzzyInference(MembershipFunction* &_input) {
 MembershipFunction & PosicaoX = _input[0];
 MembershipFunction & PosicaoY = _input[1];
 MembershipFunction & AnguloIdeal = _input[2];
 MembershipFunction *JogadaSelecionada;
 RL_ProcessoDecisao(PosicaoX, PosicaoY, AnguloIdeal, &JogadaSelecionada);
 MembershipFunction **_output = new MembershipFunction *[1];
 _output[0] = JogadaSelecionada;
 return _output;
}

void DecidirJogada::inference( double _i_PosicaoX, double _i_PosicaoY, double _i_AnguloIdeal, double *_o_JogadaSelecionada ) {
 FuzzySingleton PosicaoX(_i_PosicaoX);
 FuzzySingleton PosicaoY(_i_PosicaoY);
 FuzzySingleton AnguloIdeal(_i_AnguloIdeal);
 MembershipFunction *JogadaSelecionada;
 RL_ProcessoDecisao(PosicaoX, PosicaoY, AnguloIdeal, &JogadaSelecionada);
 if(JogadaSelecionada->getType() == MembershipFunction::CRISP) (*_o_JogadaSelecionada) = ((FuzzySingleton *) JogadaSelecionada)->getValue();
 else (*_o_JogadaSelecionada) = ((OutputMembershipFunction *) JogadaSelecionada)->defuzzify();
 delete JogadaSelecionada;
}

#endif /* _DecidirJogada_INFERENCE_ENGINE_ */



