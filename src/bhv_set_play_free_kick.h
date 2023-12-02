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

#ifndef BHV_SET_PLAY_FREE_KICK_H
#define BHV_SET_PLAY_FREE_KICK_H

#include <rcsc/geom/vector_2d.h>
#include <rcsc/player/soccer_action.h>


// our free kick

class Bhv_SetPlayFreeKick
    : public rcsc::SoccerBehavior {
public:

    Bhv_SetPlayFreeKick()
      { }

    bool execute( rcsc::PlayerAgent * agent );
    
    //============================================================================
    
    /* métodos públicos não-originais da classe */
    /*!
     * \brief Faz invocação ao mecanismo de tomada_decisao() e posterior invocação às jogadas coletivas
     */
    bool jogada_coletiva_call( rcsc::PlayerAgent * agent ); 
   
    /*! Chamada ao método difuso de seleção de jogada coletiva */ 
    int tomada_decisao(rcsc::PlayerAgent *agent);
   
    //============================================================================

private:
    rcsc::Vector2D pontoInicial;   
  
  
    void doKick( rcsc::PlayerAgent * agent );
    bool doKickWait( rcsc::PlayerAgent * agent );

    void doMove( rcsc::PlayerAgent * agent );
    
  
    /* métodos não-originais da classe a baixo... */  
    //==============================================================================
    /*! tabela pela esquerda 
     *\brief   método invocado em jogada_coletiva_call(), no execute() do arquivo bhv_setplay.cpp, linha 172
     */
     bool tabela_esq(rcsc::PlayerAgent * agent);
  
    /* \brief   método invocado em jogada_coletiva_call, no execute() do arquivo bhv_setplay.cpp, linha 172 */
    bool tabela_dir(rcsc::PlayerAgent * agent);
    
    //!método auxiliar invocado pelas jogadas de tabela para tratar o comportamento do jogador que irá cobrar a penalidade
    void passToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita);
   
    //==============================================================================
    
    /*! Jogada pelo meio com abertura pela lateral esquerda 
     *\brief   método invocado em jogada_coletiva_call(), no execute() do arquivo bhv_setplay.cpp, linha 172. Esta jogada Necessita de pelo menos 2 jogadores aliados, um se posiciona à frente do jogador que vai cobrar o tiro e o outro um pouco à esquerda desses.
     */
     bool meio_abertura_dir(rcsc::PlayerAgent * agent);
     
     //!Jogada iniciada com lançamento pelo meio com continuação em abertura pela lateral esquerda
     bool meio_abertura_esq(rcsc::PlayerAgent * agent);
     
     //!método auxiliar, invocado por "meio_abertura_dir" e "meio_abertura_esq", no arquivo bhv_set_play_free_kick.cpp
     void aberturaToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita); 
    
     //==============================================================================
     
     /*! Jogada de Abertura lateral com lançamento para o centro, vindo da Direita
      * \brief Essa jogada consiste em tentar dispersar os oponentes para que um jogador faça um lançamento para o centro, dando ao aliado maiores chances de aproximação do gol adversário
      */
      bool lateral_lanc_dir(rcsc::PlayerAgent * agent);
     
     //!Jogada iniciada com lançamento efetuando uma abertura lateral, com posterior lançamento para o meio 
     bool lateral_lanc_esq(rcsc::PlayerAgent * agent);
     
     //!método auxiliar, invocado por "lateral_lanc_dir" e "lateral_lanc_esq", no arquivo bhv_set_play_free_kick.cpp
     void centerToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita); 
    
      //==============================================================================
     
     /*! Jogada de inversão de bola para o lado oposto do campo
      * \brief Essa jogada consiste em levar a bola para uma área do campo com menor concentração de jogadores
      * jogada de inversão de bola saindo do lado direito
      */
      bool inversao_dir(rcsc::PlayerAgent * agent);
     
     //!Jogada de inversão de bola saindo do lado esquerdo
     bool inversao_esq(rcsc::PlayerAgent * agent);
     
     //!método auxiliar, invocado por "inversao_dir" e "inversao_esq", no arquivo bhv_set_play_free_kick.cpp
     void inversaoToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita); 
    
      //==============================================================================
     
      /*! Jogada de Assistência para Chute Direto
      * \brief Essa jogada consiste em sinalizar uma melhor posição para o aliado receber a bola e chutar a gol
      * jogada de inversão de bola saindo do lado direito
      */
      bool assistencia_chute_dir(rcsc::PlayerAgent * agent);
     
     //!Jogada de inversão de bola saindo do lado esquerdo
     bool assistencia_chute_esq(rcsc::PlayerAgent * agent);
     
     //!método auxiliar, invocado por "inversao_dir" e "inversao_esq", no arquivo bhv_set_play_free_kick.cpp
     void assistenciaToPlayer(rcsc::PlayerAgent * agent, bool jogada_direita);

      //==============================================================================
     
};

#endif

#ifndef _XFUZZY_HPP
#define _XFUZZY_HPP

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//           Classe Abstrata dos Numeros Fuzzy           //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class MembershipFunction {
public:
 enum Type { GENERAL, CRISP, INNER };
 virtual enum Type getType() { return GENERAL; }
 virtual double getValue() { return 0; }
 virtual double compute(double x) = 0;
 virtual ~MembershipFunction() {}
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//               Classe de um número Crisp                //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class FuzzySingleton: public MembershipFunction {
private:
 double value;

public:
 FuzzySingleton(double value) { this->value = value; }
 virtual ~FuzzySingleton() {}
 virtual double getValue() { return value; }
 virtual enum Type getType() { return CRISP; }
 virtual double compute(double x) { return (x==value? 1.0: 0.0);}
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//      Classe Abstrata do Motos de Inferencia fuzzy     //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class FuzzyInferenceEngine {
public:
 virtual double* crispInference(double* input) = 0;
 virtual double* crispInference(MembershipFunction* &input) = 0;
 virtual MembershipFunction** fuzzyInference(double* input) = 0;
 virtual MembershipFunction** fuzzyInference(MembershipFunction* &input) = 0;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//          Classe Abstrata dos Operatorset           //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class OutputMembershipFunction;

class Operatorset {
public:
 virtual double and1(double a, double b) = 0;
 virtual double or1(double a, double b) = 0;
 virtual double also(double a, double b) = 0;
 virtual double imp(double a, double b) = 0;
 virtual double not1(double a) = 0;
 virtual double very(double a) = 0;
 virtual double moreorless(double a) = 0;
 virtual double slightly(double a) = 0;
 virtual double defuz(OutputMembershipFunction &mf) = 0;
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//       funções Membership das variáveis de entrada       //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class ParamMembershipFunction {
public:
 double min, max, step;
 char *name;

 ParamMembershipFunction(double min, double max, double step);
 ParamMembershipFunction() {};
 virtual ~ParamMembershipFunction() {};
 virtual double compute_eq(double x) = 0;
 virtual double param(int i) = 0;
 virtual double center();
 virtual double basis();
 virtual double compute_smeq(double x);
 virtual double compute_greq(double x);
 double isEqual(MembershipFunction &mf);
 double isGreaterOrEqual(MembershipFunction &mf);
 double isSmallerOrEqual(MembershipFunction &mf);
 double isGreater(MembershipFunction &mf, Operatorset &op);
 double isSmaller(MembershipFunction &mf, Operatorset &op);
 double isNotEqual(MembershipFunction &mf, Operatorset &op);
 double isApproxEqual(MembershipFunction &mf, Operatorset &op);
 double isVeryEqual(MembershipFunction &mf, Operatorset &op);
 double isSlightlyEqual(MembershipFunction &mf, Operatorset &op);
 int isXflSingleton();
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//       Classe de Conclusão de uma Regra Fuzzy      //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class RuleConclusion {
private:
 double deg;
 ParamMembershipFunction *mf;
public:
 RuleConclusion(double degree, ParamMembershipFunction *mf)
  { this->deg = degree; this->mf = mf; }
 ~RuleConclusion() { delete mf; }
 double degree() { return deg; }
 double compute(double x) { return mf->compute_eq(x); }
 double center() { return mf->center(); }
 double basis() { return mf->basis(); }
 double param(int i) { return mf->param(i); }
 double min() { return mf->min; }
 double max() { return mf->max; }
 double step() { return mf->step; }
 int isDiscrete() { return mf->isXflSingleton(); }
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++//
//    função Membership da variável de saída           //
//++++++++++++++++++++++++++++++++++++++++++++++++++++++//

class OutputMembershipFunction: public MembershipFunction {
public:
 int length;
 int inputlength;
 double *input;
 RuleConclusion **conc;
private:
 Operatorset *op;
public:
 OutputMembershipFunction() {};
 OutputMembershipFunction(Operatorset *op, int length, int inputlength, double *input);
 virtual ~OutputMembershipFunction() { delete op; delete [] conc; };
 virtual enum Type getType() { return INNER; }
 double compute(double x);
 double defuzzify() { return op->defuz( (*this) ); }
 double min() { return conc[0]->min(); }
 double max() { return conc[0]->max(); }
 double step() { return conc[0]->step(); }
 int isDiscrete();
};

#endif /* _XFUZZY_HPP */


#ifndef _DecidirJogada_INFERENCE_ENGINE_HPP
#define _DecidirJogada_INFERENCE_ENGINE_HPP


//+++++++++++++++++++++++++++++++++++++//
//  MembershipFunction MF_DecidirJogada_xfl_triangle  //
//+++++++++++++++++++++++++++++++++++++//

class MF_DecidirJogada_xfl_triangle: public ParamMembershipFunction {
private:
 double a;
 double b;
 double c;

public:
 MF_DecidirJogada_xfl_triangle() {};
 virtual ~MF_DecidirJogada_xfl_triangle() {};
 MF_DecidirJogada_xfl_triangle(double min,double max,double step,double *param, int length);
 MF_DecidirJogada_xfl_triangle*dup();
 virtual double param(int _i);
 virtual double compute_eq(double x);
 virtual double compute_greq(double x);
 virtual double compute_smeq(double x);
 virtual double center();
 virtual double basis();
};

//+++++++++++++++++++++++++++++++++++++//
//  MembershipFunction MF_DecidirJogada_xfl_trapezoid  //
//+++++++++++++++++++++++++++++++++++++//

class MF_DecidirJogada_xfl_trapezoid: public ParamMembershipFunction {
private:
 double a;
 double b;
 double c;
 double d;

public:
 MF_DecidirJogada_xfl_trapezoid() {};
 virtual ~MF_DecidirJogada_xfl_trapezoid() {};
 MF_DecidirJogada_xfl_trapezoid(double min,double max,double step,double *param, int length);
 MF_DecidirJogada_xfl_trapezoid*dup();
 virtual double param(int _i);
 virtual double compute_eq(double x);
 virtual double compute_greq(double x);
 virtual double compute_smeq(double x);
 virtual double center();
 virtual double basis();
};

//+++++++++++++++++++++++++++++++++++++//
//  Operatorset OP_DecidirJogada_DefinicaoOperador //
//+++++++++++++++++++++++++++++++++++++//

class OP_DecidirJogada_DefinicaoOperador: public Operatorset {
public:
 virtual ~OP_DecidirJogada_DefinicaoOperador() {};
 virtual double and1(double a, double b);
 virtual double or1(double a, double b);
 virtual double also(double a, double b);
 virtual double imp(double a, double b);
 virtual double not1(double a);
 virtual double very(double a);
 virtual double moreorless(double a);
 virtual double slightly(double a);
 virtual double defuz(OutputMembershipFunction &mf);
};

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_DistanciaGolAdv //
//+++++++++++++++++++++++++++++++++++++//

class TP_DecidirJogada_DistanciaGolAdv {
private:
 double min;
 double max;
 double step;
public:
 MF_DecidirJogada_xfl_triangle MuitoPerto;
 MF_DecidirJogada_xfl_triangle Perto;
 MF_DecidirJogada_xfl_triangle Media;
 MF_DecidirJogada_xfl_triangle Longe;
 MF_DecidirJogada_xfl_trapezoid MuitoLonge;
 TP_DecidirJogada_DistanciaGolAdv();
};

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_PosicaoCampo //
//+++++++++++++++++++++++++++++++++++++//

class TP_DecidirJogada_PosicaoCampo {
private:
 double min;
 double max;
 double step;
public:
 MF_DecidirJogada_xfl_trapezoid Esquerda;
 MF_DecidirJogada_xfl_triangle Meio;
 MF_DecidirJogada_xfl_trapezoid Direita;
 TP_DecidirJogada_PosicaoCampo();
};

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_MelhorAngulo //
//+++++++++++++++++++++++++++++++++++++//

class TP_DecidirJogada_MelhorAngulo {
private:
 double min;
 double max;
 double step;
public:
 MF_DecidirJogada_xfl_trapezoid Esquerda;
 MF_DecidirJogada_xfl_trapezoid MeioEsquerda;
 MF_DecidirJogada_xfl_trapezoid Frontal;
 MF_DecidirJogada_xfl_trapezoid MeioDireita;
 MF_DecidirJogada_xfl_trapezoid Direita;
 TP_DecidirJogada_MelhorAngulo();
};

//+++++++++++++++++++++++++++++++++++++//
//  Tipo TP_DecidirJogada_Jogada //
//+++++++++++++++++++++++++++++++++++++//

class TP_DecidirJogada_Jogada {
private:
 double min;
 double max;
 double step;
public:
 MF_DecidirJogada_xfl_triangle InversaoEsq;
 MF_DecidirJogada_xfl_triangle LateralLancEsq;
 MF_DecidirJogada_xfl_triangle AssistenciaChuteEsq;
 MF_DecidirJogada_xfl_triangle TabelaEsq;
 MF_DecidirJogada_xfl_triangle MeioAberLatEsq;
 MF_DecidirJogada_xfl_triangle MeioAberLatDir;
 MF_DecidirJogada_xfl_triangle TabelaDir;
 MF_DecidirJogada_xfl_triangle AssistenciaChuteDir;
 MF_DecidirJogada_xfl_triangle LateralLancDir;
 MF_DecidirJogada_xfl_triangle InversaoDir;
 TP_DecidirJogada_Jogada();
};

//++++++++++++++++++++++++++++++++++++++++++++//
//  Motor de Inferência Fuzzy DecidirJogada  //
//+++++++++++++++++++++++++++++++++++++++++++//

class DecidirJogada: public FuzzyInferenceEngine {
public:
 DecidirJogada() {};
 virtual ~DecidirJogada() {};
 virtual double* crispInference(double* input);
 virtual double* crispInference(MembershipFunction* &input);
 virtual MembershipFunction** fuzzyInference(double* input);
 virtual MembershipFunction** fuzzyInference(MembershipFunction* &input);
 void inference( double _i_PosicaoX, double _i_PosicaoY, double _i_AnguloIdeal, double *_o_JogadaSelecionada );
private:
 void RL_ProcessoDecisao(MembershipFunction &PosX, MembershipFunction &PosY, MembershipFunction &AngIdeal, MembershipFunction ** _o_jogada);
};

#endif /* _DecidirJogada_INFERENCE_ENGINE_HPP */
