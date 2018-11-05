#ifndef MEDIA_TYPES_FILTER_H
#define MEDIA_TYPES_FILTER_H

#include "contenttypevalidator.h"
#include "headervalidator.h"
#include "stackstring.h"
#include "stackint.h"
#include "boundaryvalidator.h"
#include "boundarycomparator.h"
#include "extra.h"

typedef enum states
{
	NEW_LINE,    					//0
	CARRY_RETURN,					//1
	CARRY_RETURN_END_OF_HEADERS,	//2
	CHECKING_HEADER,				//3
	CHECKING_CONTENT_TYPE,			//4
	CHECKING_BOUNDARY,				//5
	CHECKING_BODY,					//6
	CHECKING_TRANSFER_ENCODING,		//7
	WAIT_FOR_NEW_LINE,				//8
	WAIT_FOR_DOT_COMMA,				//9
	WAIT_FOR_BODY,					//10
	WAIT_UNTIL_END,					//11
	WAIT_UNTIL_BOUNDARY,			//12
	WAIT_UNTIL_NEW_LINE_BODY,		//13
	IGNORE_UNTIL_NEW_LINE,			//14
	IGNORE_UNTIL_BOUNDARY,			//15
	IGNORE_CARRY_RETURN,			//16
	IGNORE_UNTIL_NEW_LINE_BODY,		//17
	IGNORE_BOUNDARY,				//18
	CARRY_RETURN_BODY,				//19
	IGNORE_CARRY_RETURN_HEADERS,	//20
	IGNORE_CARRY_RETURN_BODY,		//21
	IGNORE_UNTIL_END 				//22

}states;

typedef struct ctx
{
	/*Acciones a realizar, se utiliza una pila ya que la accion a realizar esta en el tope de esta,
	ademas en el caso de realizar otra accion se quiere recordar la ultima accion realizada previamente
	por si se quiere regresar*/
	stackint* actions;
	/*Flag de activacion de un content-type*/
	int contenttypedeclared;
	/*Flag de transfer econding encontrado*/
	int encondingdeclared;
	/*Se guarda el encoding*/
	char* encondingselected;
	/*Flag de content length declarado*/
	int contentlengthdeclared;
	/*Flag de md5sum declarado*/
	int contententmd5declared;
	/*Content-Type elegido es o no censurable, debido a que la censura es encapsulable
	no hace falta que funcione como stack*/
	char censored;
	/*Content-Type es multipart*/
	int multipart;
	/*Content-Type es message*/
	int message;
	/*Maquina de estado de content-type*/
	contentypevalidator* ctp;
	/*Maquina de estado de headers*/
	headervalidator* hv;
	/*Maquina de estado de boundaries*/
	boundaryvalidator* bv;
	/*Maquina de estados para saber si la siguiente linea es el boundary a popear de la pila*/
	boundarycomparator* bc;
	/*Stack para saber el proximo boundary a encontrar*/
	stackstring* boundaries;
	/*Buffer para informacion extra*/
	extrainformation* extra;


}ctx;

void filteremail(char* cesoredMediaTypes, char* filterMessage);
ctx* initcontext();
void destroycontext(ctx* context);
void restartcontext(ctx* context);

#endif