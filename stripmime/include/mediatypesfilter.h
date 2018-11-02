#ifndef MEDIA_TYPES_FILTER_H
#define MEDIA_TYPES_FILTER_H

#include "contenttypevalidator.h"
#include "headervalidator.h"
#include "stackstring.h"
#include "stackint.h"
#include "boundaryvalidator.h"
#include "boundarycomparator.h"

typedef enum states
{
	NEW_LINE,
	CARRY_RETURN,
	CARRY_RETURN_END_OF_HEADERS,
	CHECKING_HEADER,
	CHECKING_CONTENT_TYPE,
	CHECKING_BOUNDARY,
	CHECKING_BODY,
	CHECKING_TRANSFER_ENCODING,
	WAIT_FOR_NEW_LINE,
	WAIT_FOR_DOT_COMMA,
	WAIT_FOR_BODY,
	WAIT_UNTIL_END,
	WAIT_UNTIL_BOUNDARY,
	IGNORE_UNTIL_NEW_LINE,
	IGNORE_UNTIL_BOUNDARY,
	IGNORE_CARRY_RETURN,
	IGNORE_UNTIL_NEW_LINE_BODY,
	IGNORE_BOUNDARY,
	CARRY_RETURN_BODY,
	IGNORE_CARRY_RETURN_HEADERS,
	IGNORE_CARRY_RETURN_BODY,
	IGNORE_UNTIL_END

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
	/*Flag de content length declarado*/
	int contentlengthdeclared;
	/*Flag de md5sum declarado*/
	int contententmd5declared;
	/*Content-Type elegido es o no censurable, debido a que la censura es encapsulable
	no hace falta que funcione como stack*/
	int censored;
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


}ctx;

void filteremail(char* cesoredMediaTypes, char* filterMessage);
ctx* initcontext();
void destroycontext(ctx* context);
void restartcontext(ctx* context);

#endif