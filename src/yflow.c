/******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include <math.h>

#include "lib/stringinfo.h"
#include "libpq/pqformat.h"

#include "utils/builtins.h"
#include "utils/lsyscache.h" 
#include "catalog/pg_type.h" 
#include "funcapi.h" 

#include "wolf.h"


PG_MODULE_MAGIC;

extern int  yflow_yyparse(void *resultat);
extern void yflow_yyerror(const char *message);
extern void yflow_scanner_init(const char *str);
extern void yflow_scanner_finish(void);

// ob_tGlobales globales;

PG_FUNCTION_INFO_V1(yflow_in);
PG_FUNCTION_INFO_V1(yflow_out);
PG_FUNCTION_INFO_V1(yflow_dim);
PG_FUNCTION_INFO_V1(yflow_get_maxdim);
PG_FUNCTION_INFO_V1(yflow_init);
PG_FUNCTION_INFO_V1(yflow_grow_backward);
PG_FUNCTION_INFO_V1(yflow_grow_forward);
PG_FUNCTION_INFO_V1(yflow_finish);
PG_FUNCTION_INFO_V1(yflow_contains_oid);
PG_FUNCTION_INFO_V1(yflow_match);
PG_FUNCTION_INFO_V1(yflow_match_quality);
PG_FUNCTION_INFO_V1(yflow_checktxt);
PG_FUNCTION_INFO_V1(yflow_maxg);
PG_FUNCTION_INFO_V1(yflow_reduce);
PG_FUNCTION_INFO_V1(yflow_is_draft);
PG_FUNCTION_INFO_V1(yflow_to_matrix);
PG_FUNCTION_INFO_V1(yflow_qtts);

PG_FUNCTION_INFO_V1(yflow_show);
PG_FUNCTION_INFO_V1(yflow_to_json);
PG_FUNCTION_INFO_V1(yflow_to_jsona);

Datum yflow_in(PG_FUNCTION_ARGS);
Datum yflow_out(PG_FUNCTION_ARGS);
Datum yflow_dim(PG_FUNCTION_ARGS);
Datum yflow_get_maxdim(PG_FUNCTION_ARGS);

Datum yflow_init(PG_FUNCTION_ARGS);
Datum yflow_grow_backward(PG_FUNCTION_ARGS);
Datum yflow_grow_forward(PG_FUNCTION_ARGS);
Datum yflow_finish(PG_FUNCTION_ARGS);
Datum yflow_contains_oid(PG_FUNCTION_ARGS);
Datum yflow_match(PG_FUNCTION_ARGS);
Datum yflow_match_quality(PG_FUNCTION_ARGS);
Datum yflow_checktxt(PG_FUNCTION_ARGS);
Datum yflow_maxg(PG_FUNCTION_ARGS);
Datum yflow_reduce(PG_FUNCTION_ARGS);
Datum yflow_is_draft(PG_FUNCTION_ARGS);
Datum yflow_to_matrix(PG_FUNCTION_ARGS);
Datum yflow_qtts(PG_FUNCTION_ARGS);

Datum yflow_show(PG_FUNCTION_ARGS);
Datum yflow_to_json(PG_FUNCTION_ARGS);
Datum yflow_to_jsona(PG_FUNCTION_ARGS);

char *yflow_pathToStr(Tflow *yflow);

//void		_PG_init(void);
void		_PG_fini(void);

static double getOmegaFlow(Tflow *f);
// static bool _qua_check(int nbmin,HStore *r);

/******************************************************************************
begin and end functions called when flow.so is loaded
******************************************************************************/
/*void		_PG_init(void) {
	return;
}*/
void		_PG_fini(void) {
	return;
}

/******************************************************************************
Input/Output functions
******************************************************************************/
/* yflow = [yorder1,yorder2,....] 
where yfl = (id,oid,own,qtt_requ,qtt_prov,qtt,proba) */
Datum
yflow_in(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);
	
	Tflow 	*result;
	
	result = flowm_init();

	yflow_scanner_init(str);

	if (yflow_yyparse(&result) != 0)
		yflow_yyerror("bogus input for a yflow");

	yflow_scanner_finish();
	
	{
		TresChemin *c;
		c = flowc_maximum(result);
		pfree(c);
	}	

	//(void) flowc_maximum(result);

	PG_RETURN_TFLOW(result);
}
/******************************************************************************
provides a string representation of yflow 
When internal is set, it gives complete representation of the yflow,
adding status and flowr[.]
******************************************************************************/
char *yflow_ndboxToStr(Tflow *yflow,bool internal) {
	StringInfoData 	buf;
	int	dim = yflow->dim;
	int	i;

	initStringInfo(&buf);

	if(internal) {
		appendStringInfo(&buf, "YFLOW ");
	}
	appendStringInfoChar(&buf, '[');
	if(dim >0) {
		for (i = 0; i < dim; i++)
		{	
			Tfl *s = &yflow->x[i];
		
			if(i != 0) appendStringInfoChar(&buf, ',');

			// type,id,oid,own,qtt_requ,qtt_prov,qtt,proba
			appendStringInfo(&buf, "(%i, ", s->type);
			appendStringInfo(&buf, "%i, ", s->id);
			appendStringInfo(&buf, "%i, ", s->oid);
			appendStringInfo(&buf, "%i, ", s->own);
			appendStringInfo(&buf, INT64_FORMAT ", ", s->qtt_requ);
			appendStringInfo(&buf, INT64_FORMAT ", ", s->qtt_prov);
			appendStringInfo(&buf, INT64_FORMAT ", ", s->qtt);
		
			if(internal)
				appendStringInfo(&buf,"%f :" INT64_FORMAT ")",s->proba, s->flowr);
			else 
				appendStringInfo(&buf, "%f)", s->proba);
		}
	}
	appendStringInfoChar(&buf, ']');
	if(internal)
		appendStringInfoChar(&buf, '\n');
	
	return buf.data;
}


/******************************************************************************
******************************************************************************/
Datum yflow_out(PG_FUNCTION_ARGS)
{
	Tflow	*_yflow;
	char 	*_res;
	
	_yflow = PG_GETARG_TFLOW(0);					
	_res = yflow_ndboxToStr(_yflow,false);

	PG_RETURN_CSTRING(_res);
}
/******************************************************************************
******************************************************************************/
Datum yflow_dim(PG_FUNCTION_ARGS)
{
	Tflow	*f;
	int32	dim;
	
	f = PG_GETARG_TFLOW(0);
	dim = f->dim;
	
	if(dim > FLOW_MAX_DIM)
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("flow->dim not <=%i",FLOW_MAX_DIM)));
	PG_RETURN_INT32(dim);
}
/******************************************************************************
******************************************************************************/
Datum yflow_get_maxdim(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(FLOW_MAX_DIM);
}

/******************************************************************************
******************************************************************************/
Datum yflow_show(PG_FUNCTION_ARGS) {
	Tflow	*X;
	char *str;
	
	X = PG_GETARG_TFLOW(0);
	// elog(WARNING,"yflow_show: %s",yflow_ndboxToStr(X,true));
	//str = flowc_cheminToStr(c);
	str = yflow_ndboxToStr(X,true);
	PG_RETURN_CSTRING(str);
/*
	c = flowc_maximum(X);
	str = flowc_cheminToStr(c);
	pfree(c);
	PG_RETURN_CSTRING(str);
*/
}
/******************************************************************************
******************************************************************************/
Datum yflow_to_json(PG_FUNCTION_ARGS) {
	Tflow *yflow = PG_GETARG_TFLOW(0);
	StringInfoData 	buf;
	int	dim = yflow->dim;
	int	i;
	
	if(dim ==1)
			ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				errmsg("flow with dim =1")));

	initStringInfo(&buf);

	appendStringInfoChar(&buf, '[');
	for (i = 0; i < dim; i++)
	{	
		Tfl *s = &yflow->x[i];
	
		if(i != 0) appendStringInfo(&buf, ",\n");

		// type,id,oid,own,qtt_requ,qtt_prov,qtt,proba
		appendStringInfo(&buf, "{\"type\":%i, ", s->type);
		appendStringInfo(&buf, "\"id\":%i, ", s->id);
		appendStringInfo(&buf, "\"oid\":%i, ", s->oid);
		appendStringInfo(&buf, "\"own\":%i, ", s->own);
		appendStringInfo(&buf, "\"qtt_requ\":" INT64_FORMAT ", ", s->qtt_requ);
		appendStringInfo(&buf, "\"qtt_prov\":" INT64_FORMAT ", ", s->qtt_prov);
		appendStringInfo(&buf, "\"qtt\":" INT64_FORMAT ", ", s->qtt);
	
		appendStringInfo(&buf,"\"proba\":%.10e,\"flowr\":" INT64_FORMAT "}",s->proba, s->flowr);
	}
	appendStringInfoChar(&buf, ']');
	
	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}
/******************************************************************************
******************************************************************************/
Datum yflow_to_jsona(PG_FUNCTION_ARGS) {
	Tflow *yflow = PG_GETARG_TFLOW(0);
	StringInfoData 	buf;
	int	dim = yflow->dim;
	int	i;

	initStringInfo(&buf);

	appendStringInfoChar(&buf, '[');
	if(dim ==1)
			ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				errmsg("yflow_to_jsona: flow with dim =1")));
	for (i = 0; i < dim; i++)
	{	
		Tfl *s = &yflow->x[i];
	
		if(i != 0) appendStringInfo(&buf, ",\n");

		// type,id,oid,own,qtt_requ,qtt_prov,qtt,proba
		appendStringInfo(&buf, "{\"type\":%i, ", s->type);
		appendStringInfo(&buf, "\"id\":%i, ", s->id);
		appendStringInfo(&buf, "\"oid\":%i, ", s->oid);
		appendStringInfo(&buf, "\"own\":%i, ", s->own);
		/*
		if((i == dim-1) && (FLOW_IS_IGNOREOMEGA(yflow))) {
			Tfl *sp = &yflow->x[i-1];
			
			appendStringInfo(&buf, "\"qtt_requ\":" INT64_FORMAT ", ", sp->flowr);
			appendStringInfo(&buf, "\"qtt_prov\":" INT64_FORMAT ", ", s->flowr);
			appendStringInfo(&buf, "\"qtt\":" INT64_FORMAT ", ", s->flowr);
		} else {
			appendStringInfo(&buf, "\"qtt_requ\":" INT64_FORMAT ", ", s->qtt_requ);
			appendStringInfo(&buf, "\"qtt_prov\":" INT64_FORMAT ", ", s->qtt_prov);
			appendStringInfo(&buf, "\"qtt\":" INT64_FORMAT ", ", s->qtt);
		}
		*/
		appendStringInfo(&buf, "\"qtt_requ\":" INT64_FORMAT ", ", s->qtt_requ);
		appendStringInfo(&buf, "\"qtt_prov\":" INT64_FORMAT ", ", s->qtt_prov);
		appendStringInfo(&buf, "\"qtt\":" INT64_FORMAT ", ", s->qtt);
			
		appendStringInfo(&buf,"\"flowr\":" INT64_FORMAT "}", s->flowr);
	}
	appendStringInfoChar(&buf, ']');
	
	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

/******************************************************************************
******************************************************************************/
char * yflow_statusToStr (Tstatusflow s){
	switch(s) {
	case noloop: return "noloop";
	case draft: return "draft";
	case refused: return "refused";
	case undefined: return "undefined";
	case empty: return "empty";
	default: return "unknown status!";
	}
}
/******************************************************************************
******************************************************************************/
char * yflow_typeToStr (int32 t){
	switch(ORDER_TYPE(t)) {
	case ORDER_LIMIT: return "ORDER_LIMIT";
	case ORDER_BEST: return "ORDER_BEST";
	default: return "unknown type!";
	}
}
/******************************************************************************
yflow_get()
******************************************************************************/
static Tflow* _yflow_get(Tfl *o,Tflow *f, bool before) {
	Tflow	*result;
	short 	dim = f->dim,i;
	bool	found = false;
	
	if(dim > (FLOW_MAX_DIM-1))
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to extend a yflow out of range")));
			
	obMRange(i,dim) {

		if(f->x[i].oid == o->oid) {
			found = true;
			break;
		} else if(f->x[i].id == o->id) //  && f->x[i].oid != o->oid
			ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				errmsg("same order with different oid")));
	}
		
	if (found)
		result = flowm_copy(f);
	else
		result = flowm_cextends(o,f,before);

	#ifdef GL_WARNING_GET
		elog(WARNING,"_yflow_get %s",yflow_pathToStr(result));
	#endif
	return result;	
	
}

/******************************************************************************
 flow_init(order) ->path = [ord] distance[0]=0 
******************************************************************************/

Datum yflow_init(PG_FUNCTION_ARGS) {
	
	Datum	dnew = (Datum) PG_GETARG_POINTER(0);
	Torder	new;
	Tflow	*f,*result;
	Tfl		fnew;
	
	yorder_get_order(dnew,&new);
	yorder_to_fl(&new,&fnew);
	
	f = flowm_init();
	result = _yflow_get(&fnew,f,true);
	result->x[0].proba = -1.0;
	result->x[0].flowr = 0;

	PG_RETURN_TFLOW(result);

}
/******************************************************************************
 flow_grow_backward(new,debut,path) -> path = new || path avec la distance(new,debut) 
******************************************************************************/
Datum yflow_grow_backward(PG_FUNCTION_ARGS) {
	
	Datum	dnew = (Datum) PG_GETARG_POINTER(0);
	Datum	ddebut = (Datum) PG_GETARG_POINTER(1);
	Tflow	*f = PG_GETARG_TFLOW(2);
	Torder	new,debut;
	Tfl		fnew;
	Tflow	*result;
	short	dim = f->dim;
	
	if(dim == 0 )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to extend a yflow that is empty")));
	
	yorder_get_order(ddebut,&debut);
	if(f->x[0].id != debut.id )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to grow a yflow with an order %i that is not the begin of the flow %s",debut.id,yflow_ndboxToStr(f,false))));
	
	yorder_get_order(dnew,&new);		
	if(!yorder_match(&new,&debut))
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to grow a yflow with an order that is not matching the begin of the flow")));

			
	yorder_to_fl(&new,&fnew);
	result = _yflow_get(&fnew,f,true);
	result->x[0].proba = yorder_match_proba(&new,&debut);
	result->x[0].flowr = 0;
	
	PG_FREE_IF_COPY(f, 2);
	PG_RETURN_TFLOW(result);

}
/******************************************************************************
 flow_grow_backward(new,debut,path) -> path = new || path avec la distance(new,debut) 
******************************************************************************/
Datum yflow_grow_forward(PG_FUNCTION_ARGS) {
	
	Datum	dnew = (Datum) PG_GETARG_POINTER(0);
	Datum	dfin = (Datum) PG_GETARG_POINTER(1);
	Tflow	*f = PG_GETARG_TFLOW(2);
	Torder	new,fin;
	Tfl		fnew;
	Tflow	*result;
	short	dim = f->dim;
	
	if(dim == 0 )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to extend a yflow that is empty")));
	
	yorder_get_order(dfin,&fin);
	if(f->x[dim-1].id != fin.id )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to grow a yflow with an order %i that is not the end of the flow %s",fin.id,yflow_ndboxToStr(f,false))));
	
	yorder_get_order(dnew,&new);		
	if(!yorder_match(&fin,&new))
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to grow a yflow with an order that is not matching the end of the flow")));

			
	yorder_to_fl(&new,&fnew);
	result = _yflow_get(&fnew,f,false);
	result->x[dim-1].proba = yorder_match_proba(&fin,&new);
	result->x[dim-1].flowr = 0;
	
	PG_FREE_IF_COPY(f, 2);
	PG_RETURN_TFLOW(result);

}
/******************************************************************************
 flow_finish(debut,path,fin) -> -> path idem, distance[0] la distance(fin,debut) 
******************************************************************************/

Datum yflow_finish(PG_FUNCTION_ARGS) {
	
	Datum	ddebut = (Datum) PG_GETARG_POINTER(0);
	Tflow	*f = PG_GETARG_TFLOW(1);
	Datum	dfin = (Datum) PG_GETARG_POINTER(2);
	
	Torder	debut,fin;
	Tflow	*result;
	short	dim = f->dim;

	if(dim < 2 )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to finish a yflow that has less than two partners")));
				
	yorder_get_order(dfin,&fin);
	if(f->x[dim-1].id != fin.id )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to finish a yflow with an order that is not the one finishing the flow")));
			
	yorder_get_order(ddebut,&debut);
	if(f->x[0].id != debut.id )
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to finish a yflow with an order %i that is not the one starting %s",debut.id,yflow_ndboxToStr(f,false) )));

	if(!yorder_match(&fin,&debut))
    		ereport(ERROR,
			(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
			errmsg("attempt to finish a yflow where the end is not matching the begin")));
							
	result = flowm_copy(f);
	result->x[result->dim-1].proba = yorder_match_proba(&fin,&debut);

	{
		TresChemin *c;
		c = flowc_maximum(result);
		pfree(c);
	}	

	PG_FREE_IF_COPY(f, 1);
	PG_RETURN_TFLOW(result);
}
/******************************************************************************
 flow_contains_id(flow) -> true when some order of the flow have the same oid 
******************************************************************************/
Datum yflow_contains_oid(PG_FUNCTION_ARGS) {
	int32	oid = PG_GETARG_INT32(0);
	Tflow	*f = PG_GETARG_TFLOW(1);
	short 	i,dim = f->dim;
	bool	result = false;

	obMRange(i,dim)
		if(f->x[i].oid == oid) {
			result = true;
			break;
		} 

	PG_FREE_IF_COPY(f, 1);
	PG_RETURN_BOOL(result);
}
/******************************************************************************
 flow_match_order(yorder,yorder) -> true when they match 
******************************************************************************/
Datum yflow_match(PG_FUNCTION_ARGS) {
	
	Datum	dprev = (Datum) PG_GETARG_POINTER(0);
	Datum	dnext = (Datum) PG_GETARG_POINTER(1);
	Torder	prev,next;

	yorder_get_order(dprev,&prev);
	yorder_get_order(dnext,&next);

	PG_RETURN_BOOL(yorder_match(&prev,&next));
}
/******************************************************************************
 flow_match_quality(qua_requ,qua_prov) -> true when they match 
******************************************************************************/
Datum yflow_match_quality(PG_FUNCTION_ARGS) {
	
	Datum	prov = (Datum) PG_DETOAST_DATUM(PG_GETARG_POINTER(0));
	Datum	requ = (Datum) PG_DETOAST_DATUM(PG_GETARG_POINTER(1));
	//HStore *prov = PG_GETARG_HS(0);
	//HStore *requ = PG_GETARG_HS(1);

	PG_RETURN_BOOL(yorder_match_quality(prov,requ));
}
/******************************************************************************
 yflow_checktxt(text) = res
  res & 1 notempty
  res & 2 prefixed not empty
  res & 4 suffixe not empty
  
******************************************************************************/
Datum yflow_checktxt(PG_FUNCTION_ARGS) {
	
	Datum	texte = (Datum) PG_DETOAST_DATUM(PG_GETARG_POINTER(0));

	PG_RETURN_INT32(yorder_checktxt(texte));
}

/******************************************************************************
CREATE FUNCTION ywolf_maxg(yorder[] w0,yorder[] w1)
RETURNS yorder[]
AS 'exampleText.so'
LANGUAGE C IMMUTABLE STRICT;
returns the w0 if w0>w1 otherwise w1
******************************************************************************/
Datum yflow_maxg(PG_FUNCTION_ARGS)
{
	Tflow	*f0 = PG_GETARG_TFLOW(0);
	Tflow	*f1 = PG_GETARG_TFLOW(1);
	
	double			_rank0,_rank1;
	bool			_sup = false;
	
	_rank0 = getOmegaFlow(f0);
	if(_rank0 == 0.0)
		goto _end;
		
	_rank1 = getOmegaFlow(f1);
	if(_rank1 == 0.0)
		goto _end;	
	
	// comparing weight
	if(_rank0 == _rank1) {
		/* rank are geometric means of proba. Since 0 <=proba <=1
		if it was a product,large cycles would be penalized.
		*/
		short	dim0 = f0->dim,dim1 = f1->dim,i;
		_rank0 = 1.0;
		obMRange(i,dim0) 
			_rank0 *=  (double) f0->x[i].proba;				
		_rank0 = pow(_rank0,1.0/(double) dim0);
		
		_rank1 = 1.0;
		obMRange(i,dim1) 
			_rank1 *=  (double) f1->x[i].proba;
		_rank1 = pow(_rank1,1.0/(double) dim1);	
		
		_sup = _rank0 > _rank1;				
		
	} else 
		_sup = _rank0 > _rank1;
	
	//elog(WARNING,"yflow_maxg: wolf0: %s",ywolf_allToStr(wolf0));
	//elog(WARNING,"yflow_maxg: wolf1: %s",ywolf_allToStr(wolf1));
	//elog(WARNING,"ywolf_maxg: rank0=%f,rank1=%f",_rank0,_rank1);
		
_end:
	if(_sup) {
		PG_FREE_IF_COPY(f1, 1);
		PG_RETURN_TFLOW(f0);
	} else {
		PG_FREE_IF_COPY(f0, 0);
		PG_RETURN_TFLOW(f1);
	}	
}
/******************************************************************************

******************************************************************************/
static double getOmegaFlow(Tflow *f) {
	short i,k,dim = f->dim;
	double	_rank = 1.0;
	
	if(dim < 2) // f is empty
		return 0.0;
    
    k = dim;
	if(FLOW_IS_IGNOREOMEGA(f)) 
	    k -=1;

	obMRange(i,k) {
		Tfl *b = &f->x[i];
		if(b->flowr <=0 ) 
			return 0.0;	// flow is not a draft
		_rank *=  GET_OMEGA_P(b);				
	}
	return _rank;
		
} 

/******************************************************************************
yflow = yflow_reduce(r yflow,f1 yflow)
if (r and f1 are drafts) 
	when r->x[i].id == f1->x[j].id 
		if r->x[i].qtt >= f1->x[j].flowr
			r->x[i].qtt -= f1->x[j].flowr
		else 
			error
if (r or f1 is not in (empty,draft))
	error
******************************************************************************/
Datum yflow_reduce(PG_FUNCTION_ARGS)
{
	Tflow	*f0 = PG_GETARG_TFLOW(0);
	Tflow	*f1 = PG_GETARG_TFLOW(1);
	bool	freezeOmega = PG_GETARG_BOOL(2);
	Tflow	*r;
	short 	i,j;
	TresChemin *c;
	Tfl *lastr;
			 
	r = flowm_copy(f0);
	
	// sanity check
	if(r->x[r->dim-1].id != f1->x[f1->dim-1].id)
			ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						errmsg("yflow_reduce: the last nodes are not the same")));
	
	// r->x[.].qtt -= f1->x[.].flowr
	obMRange(i,f1->dim) {
		obMRange(j,r->dim) { 
			Tfl *or  = &r->x[j];
			
			if(or->oid != f1->x[i].oid) 
				continue;
			// for all orders common to the flows
			if(ORDER_IS_NOQTTLIMIT(or->type))
				continue;
			// quantity is limited
			
			if(or->qtt < f1->x[i].flowr) 
		    	ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					errmsg("yflow_reduce: the flow is greater than available")));

			or->qtt -= f1->x[i].flowr;

		}		
	}

	lastr  = &r->x[r->dim-1];
	if(ORDER_IS_IGNOREOMEGA(lastr->type)) {
		Tfl *lastf1 = &f1->x[f1->dim-1];

		// omega is set
		lastr->qtt_prov = lastf1->qtt_prov;
		lastr->qtt_requ = lastf1->qtt_requ;
		
		// IGNOREOMEGA is reset	
		if(freezeOmega)
			lastr->type = lastr->type & (~ORDER_IGNOREOMEGA);	
	}
	/*
	if(lastr->id == 10013)
		elog(WARNING,"yflow_reduce: r=%s",yflow_ndboxToStr(r,true));
	*/

	c = flowc_maximum(r);
	/*
	if(lastr->id == 10013)
		elog(WARNING,"yflow_reduce: OK");
	*/
	pfree(c);
	
	PG_FREE_IF_COPY(f0, 0);
	PG_FREE_IF_COPY(f1, 1);
	PG_RETURN_TFLOW(r);

}

/******************************************************************************
******************************************************************************/
Datum yflow_is_draft(PG_FUNCTION_ARGS)
{
	Tflow	*f = PG_GETARG_TFLOW(0);
	bool	isdraft = true;
	short 	i,_dim = f->dim;
		
	if(_dim < 2) PG_RETURN_BOOL(false);

	obMRange(i,_dim) {
		if (f->x[i].flowr <=0) {
			isdraft = false;
			break;
		}
	}
	PG_FREE_IF_COPY(f, 0);
	PG_RETURN_BOOL(isdraft);
}
/******************************************************************************
returns a matrix int8[i][j] of i lines of nodes, where a node is:
	[id,own,oid,qtt_requ,qtt_prov,qtt,flowr]
******************************************************************************/
Datum yflow_to_matrix(PG_FUNCTION_ARGS)
{
#define DIMELTRESULT 7

	Tflow	   *flow;
	ArrayType	*result;

	int16       typlen;
	bool        typbyval;
	char        typalign;
	int         ndims = 2;
	int         dims[2] = {0,DIMELTRESULT};
	int         lbs[2] = {1,1};

	int 		_dim,_i;
	Datum		*_datum_out;
	bool		*_null_out;
		
	flow = PG_GETARG_TFLOW(0);	

	_dim = flow->dim;

	if(_dim == 0) {
		result = construct_empty_array(INT8OID);
		PG_RETURN_ARRAYTYPE_P(result);
	}
	
	_datum_out = palloc(sizeof(Datum) * _dim * DIMELTRESULT);
	_null_out =  palloc(sizeof(bool)  * _dim * DIMELTRESULT);
	
	//  id,own,nr,qtt_requ,np,qtt_prov,qtt,flowr
	obMRange(_i,_dim) {
		int _j = _i * DIMELTRESULT;
		_null_out[_j+0] = false; _datum_out[_j+0] = Int64GetDatum((int64) flow->x[_i].id);
		_null_out[_j+1] = false; _datum_out[_j+1] = Int64GetDatum((int64) flow->x[_i].own);
		_null_out[_j+2] = false; _datum_out[_j+2] = Int64GetDatum((int64) flow->x[_i].oid);
		_null_out[_j+3] = false; _datum_out[_j+3] = Int64GetDatum(flow->x[_i].qtt_requ);
		_null_out[_j+4] = false; _datum_out[_j+4] = Int64GetDatum(flow->x[_i].qtt_prov);
		_null_out[_j+5] = false; _datum_out[_j+5] = Int64GetDatum(flow->x[_i].qtt);
		_null_out[_j+6] = false; _datum_out[_j+6] = Int64GetDatum(flow->x[_i].flowr);
	}

	dims[0] = _dim;

	// get required info about the INT8 
	get_typlenbyvalalign(INT8OID, &typlen, &typbyval, &typalign);

	// now build the array 
	result = construct_md_array(_datum_out, _null_out, ndims, dims, lbs,
		                INT8OID, typlen, typbyval, typalign);
	PG_FREE_IF_COPY(flow,0);
	PG_RETURN_ARRAYTYPE_P(result);
}
/******************************************************************************
aggregate function [qtt_in,qtt_out] = yflow_qtts(yflow)
with: qtt_out=flow[dim-1] and qtt_in=flow[dim-2]
******************************************************************************/
Datum yflow_qtts(PG_FUNCTION_ARGS)
{
	Tflow	*f = PG_GETARG_TFLOW(0);
	Datum	*_datum_out;
	bool	*_isnull;
	
	ArrayType  *result;
	int16       _typlen;
	bool        _typbyval;
	char        _typalign;
	int         _dims[1];
	int         _lbs[1];
	int64		_qtt_in =0,_qtt_out =0,_qtt_requ =0,_qtt_prov = 0,_qtt = 0;//,_id_give = 0;
	short		_i;
	bool		_isDraft = true;
	
	obMRange(_i,f->dim) {
		if(f->x[_i].flowr <=0) {
			_isDraft = false;
		}
	}
	if(_isDraft) {
		//int64	_in,_out;
		//elog(WARNING,"_qtt_in=%li _qtt_out=%li",f->x[f->dim-2].flowr,f->x[f->dim-1].flowr);
		_qtt_in  = Int64GetDatum(f->x[f->dim-2].flowr);
		_qtt_out = Int64GetDatum(f->x[f->dim-1].flowr);
		_qtt_requ = Int64GetDatum(f->x[f->dim-1].qtt_requ);
		_qtt_prov = Int64GetDatum(f->x[f->dim-1].qtt_prov);
		_qtt = Int64GetDatum(f->x[f->dim-1].qtt);
		//_id_give = Int64GetDatum(f->x[f->dim-2].id);
		//_in = DatumGetInt64(_qtt_in);
		//_out = DatumGetInt64(_qtt_out);
		//elog(WARNING,"_in=%li _out=%li",_in,_out);
	} else {
			ereport(WARNING,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("yflow_qtts: the flow should be draft")));
	}
	_datum_out = palloc(sizeof(Datum) * 5);
	_isnull = palloc(sizeof(bool) * 5);
	_datum_out[0] = _qtt_in;	_isnull[0] = false;
	_datum_out[1] = _qtt_out;	_isnull[1] = false;
	_datum_out[2] = _qtt_requ;	_isnull[2] = false;
	_datum_out[3] = _qtt_prov;	_isnull[3] = false;
	_datum_out[4] = _qtt;		_isnull[4] = false;
    //_datum_out[5] = _id_give;	_isnull[5] = false;
    
	_dims[0] = 5;
	_lbs[0] = 1;
				 
	/* get required info about the INT8 */
	get_typlenbyvalalign(INT8OID, &_typlen, &_typbyval, &_typalign);

	/* now build the array */
	result = construct_md_array(_datum_out, _isnull, 1, _dims, _lbs,
		                INT8OID, _typlen, _typbyval, _typalign);
	PG_FREE_IF_COPY(f,0);
	PG_RETURN_ARRAYTYPE_P(result);
	
}

PG_FUNCTION_INFO_V1(yflow_checkquaownpos);
Datum yflow_checkquaownpos(PG_FUNCTION_ARGS);
/******************************************************************************
CREATE FUNCTION yflow_checkquaownpos(_own text, _qua_requ text,_pos_requ point, _qua_prov text, _pos_prov point, _dist float8)
RETURNS int
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT;
******************************************************************************/
#define TXT_IS_FLAGSET(__txt,__flag) ((yorder_checktxt(__txt) & __flag) == __flag)
Datum yflow_checkquaownpos(PG_FUNCTION_ARGS) {

    Datum	_own = (Datum) PG_DETOAST_DATUM(PG_GETARG_POINTER(0));
    Datum	_qua_requ = (Datum) PG_DETOAST_DATUM(PG_GETARG_POINTER(1));
    // HStore *_qua_requ = PG_GETARG_HS(1);
    Point	*_pos_requ = (Point *) PG_GETARG_POINTER(2);
    Datum	_qua_prov = (Datum) PG_DETOAST_DATUM(PG_GETARG_POINTER(3));
    // HStore *_qua_prov = PG_GETARG_HS(3);
    Point	*_pos_prov = (Point *) PG_GETARG_POINTER(4);
    double _dist = PG_GETARG_FLOAT8(5);

    if(! ( 
           TXT_IS_FLAGSET(_qua_prov,TEXT_NOT_EMPTY) 
        && TXT_IS_FLAGSET(_qua_requ,TEXT_NOT_EMPTY)
        && TXT_IS_FLAGSET(_own,TEXT_NOT_EMPTY)
    )) 
        PG_RETURN_INT32(-5);

	if(	earth_check_point(_pos_prov)!=0 ) PG_RETURN_INT32(-6);
	if (earth_check_point(_pos_requ)!=0 ) PG_RETURN_INT32(-7);
	if (earth_check_dist(_dist)!=0 ) PG_RETURN_INT32(-8);
	        
    if(yorder_match_quality(_qua_prov,_qua_requ) 
        && earth_match_position(_dist,_pos_prov,_pos_requ)
    ) 
        PG_RETURN_INT32(-9);
        
	PG_RETURN_INT32(0);
}

/******************************************************************************
qua correct if number of keys >= nbmin
******************************************************************************/
/* static bool _qua_check(int nbmin,HStore *r) {
	int			cntr = HS_COUNT(r); 
	
	return (cntr >= nbmin);
} 

PG_FUNCTION_INFO_V1(yflow_quacheck);
Datum yflow_quacheck(PG_FUNCTION_ARGS);
Datum yflow_quacheck(PG_FUNCTION_ARGS)
{
	HStore	*h = PG_GETARG_HS(0);
	int32	nbmin = PG_GETARG_INT32(1);
	bool	res;

	res  = _qua_check(nbmin,h);

	PG_FREE_IF_COPY(h, 0);
	PG_RETURN_BOOL(res);
}
*/



