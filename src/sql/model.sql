\set ECHO none

drop schema IF EXISTS test0_6_1 CASCADE;
CREATE SCHEMA test0_6_1;
SET search_path TO test0_6_1;

SET client_min_messages = warning;
SET log_error_verbosity = terse;

drop extension if exists btree_gin cascade;
create extension btree_gin with version '1.0';

drop extension if exists flow cascade;
create extension flow with version '1.0';

--------------------------------------------------------------------------------
-- main constants of the model
--------------------------------------------------------------------------------
create table tconst(
	name text UNIQUE not NULL,
	value	int,
	PRIMARY KEY (name)
);

--------------------------------------------------------------------------------
INSERT INTO tconst (name,value) VALUES 
	('MAXCYCLE',6), --must be less than yflow_get_maxdim()
	('MAXPATHFETCHED',1024),
	-- ('RANK_NORMALIZATION',2|4),
	-- it is the version of the model, not that of the extension
	('VERSION-X.y.z',0),
	('VERSION-x.Y.y',6),
	('VERSION-x.y.Z',0);
	
	-- maximum number of paths of the set on which the competition occurs
	-- ('MAXBRANCHFETCHED',20);


--------------------------------------------------------------------------------
-- fetch a constant, and verify consistancy
CREATE FUNCTION fgetconst(_name text) RETURNS int AS $$
DECLARE
	_ret int;
BEGIN
	SELECT value INTO _ret FROM tconst WHERE name=_name;
	IF(NOT FOUND) THEN
		RAISE EXCEPTION 'the const % is not found',_name USING ERRCODE= 'YA002';
	END IF;
	IF(_name = 'MAXCYCLE' AND _ret >yflow_get_maxdim()) THEN
		RAISE EXCEPTION 'MAXVALUE must be <=%',yflow_get_maxdim() USING ERRCODE='YA002';
	END IF;
	RETURN _ret;
END; 
$$ LANGUAGE PLPGSQL STABLE;

--------------------------------------------------------------------------------
-- definition of roles
--	batch <- role_bo
--	user <- client <- role_co 
--------------------------------------------------------------------------------
CREATE FUNCTION _create_roles() RETURNS int AS $$
DECLARE
	_rol text;
BEGIN
	BEGIN 
		CREATE ROLE role_co; 
	EXCEPTION WHEN duplicate_object THEN
		NULL;	
	END;
	ALTER ROLE role_co NOINHERIT NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION;
	
	BEGIN 
		CREATE ROLE role_bo; 
	EXCEPTION WHEN duplicate_object THEN
		NULL;	
	END;
	ALTER ROLE role_bo NOINHERIT NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION;
	
	BEGIN 
		CREATE ROLE role_client;
	EXCEPTION WHEN duplicate_object THEN
		NULL;
	END;
	ALTER ROLE role_client INHERIT;
	GRANT role_co TO role_client;

	-- a single connection is allowed	
	BEGIN 
		CREATE ROLE role_batch;
	EXCEPTION WHEN duplicate_object THEN
		NULL;
	END;
	ALTER ROLE batch NOINHERIT NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION; 
	ALTER ROLE batch LOGIN CONNECTION LIMIT 1;
	ALTER ROLE batch INHERIT;
	GRANT role_bo TO role_batch;
	
	BEGIN 
		CREATE ROLE admin;
	EXCEPTION WHEN duplicate_object THEN
		NULL;
	END;
	ALTER ROLE admin NOINHERIT NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION; 
	ALTER ROLE admin LOGIN CONNECTION LIMIT 1;
	RETURN 0;
END; 
$$ LANGUAGE PLPGSQL;

SELECT _create_roles();
DROP FUNCTION _create_roles();

--------------------------------------------------------------------------------
CREATE FUNCTION fifo_init(_name text) RETURNS void AS $$
BEGIN
	EXECUTE 'CREATE INDEX ' || _name || '_id_idx ON ' || _name || '((id) ASC)';
END; 
$$ LANGUAGE PLPGSQL;

--------------------------------------------------------------------------------
-- trigger before insert on some tables
--------------------------------------------------------------------------------
CREATE FUNCTION ftime_updated() 
	RETURNS trigger AS $$
BEGIN
	IF (TG_OP = 'INSERT') THEN
		NEW.created := statement_timestamp();
	ELSE 
		NEW.updated := statement_timestamp();
	END IF;
	RETURN NEW;
END;
$$ LANGUAGE PLPGSQL;
comment on FUNCTION ftime_updated() is 
'trigger updating fields created and updated';

--------------------------------------------------------------------------------
CREATE FUNCTION _reference_time(_table text) RETURNS int AS $$
DECLARE
	_res int;
BEGIN
	
	EXECUTE 'ALTER TABLE ' || _table || ' ADD created timestamp';
	EXECUTE 'ALTER TABLE ' || _table || ' ADD updated timestamp';
	EXECUTE 'CREATE TRIGGER trig_befa_' || _table || ' BEFORE INSERT
		OR UPDATE ON ' || _table || ' FOR EACH ROW
		EXECUTE PROCEDURE ftime_updated()' ; 
	RETURN 0;
END; 
$$ LANGUAGE PLPGSQL;

--------------------------------------------------------------------------------
CREATE FUNCTION _grant_read(_table text) RETURNS void AS $$

BEGIN 
	EXECUTE 'GRANT SELECT ON TABLE ' || _table || ' TO role_co,role_bo';
	RETURN;
END; 
$$ LANGUAGE PLPGSQL;
SELECT _grant_read('tconst');

--------------------------------------------------------------------------------
create domain dquantity AS int8 check( VALUE>0);


--------------------------------------------------------------------------------
-- ORDER
--------------------------------------------------------------------------------
create domain dtypeorder AS int check(VALUE>0 OR VALUE<7);
-- bbc
-- bb 1 order limit
-- bb 2 order best
-- c set when quote

create table torder ( 
	usr text,
    ord yorder,
    created timestamp not NULL,
    updated timestamp
);

SELECT _grant_read('torder');

comment on table torder is 'description of orders';

create index torder_qua_prov_idx on torder using gin(((ord).qua_prov) text_ops);
create index torder_id_idx on torder(((ord).id));
create index torder_oid_idx on torder(((ord).oid));


--------------------------------------------------------------------------------
-- TOWNER
--------------------------------------------------------------------------------
create table towner (
    id serial UNIQUE not NULL,
    name text UNIQUE not NULL,
    PRIMARY KEY (id),
    UNIQUE(name),
    CHECK(	
    	char_length(name)>0 
    )
);
comment on table towner is 'owners of values exchanged';
alter sequence towner_id_seq owned by towner.id;
create index towner_name_idx on towner(name);
SELECT _reference_time('towner');
SELECT _grant_read('towner');


-- id,own,oid,qtt_requ,qua_requ,qtt_prov,qua_prov,qtt
create view vorder as
select (o.ord).id as id,w.name as own,(o.ord).oid as oid,
		(o.ord).qtt_requ as qtt_requ,(o.ord).qua_requ as qua_requ,
		(o.ord).qtt_prov as qtt_prov,(o.ord).qua_prov as qua_prov,
		(o.ord).qtt as qtt, o.created as created
from torder o left join towner w on ((o.ord).own=w.id) where o.usr=current_user;
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
CREATE FUNCTION fgetowner(_name text) RETURNS int AS $$
DECLARE
	_wid int;
BEGIN
	LOOP
		SELECT id INTO _wid FROM towner WHERE name=_name;
		IF found THEN
			return _wid;
		END IF;
		BEGIN
			if(char_length(_name)<1) THEN
				RAISE EXCEPTION 'Owner s name cannot be empty' USING ERRCODE='YU001';
			END IF;
			INSERT INTO towner (name) VALUES (_name) RETURNING id INTO _wid;
			-- RAISE NOTICE 'owner % created',_name;
			return _wid;
		EXCEPTION WHEN unique_violation THEN
			--
		END;
	END LOOP;
END;
$$ LANGUAGE PLPGSQL;
--------------------------------------------------------------------------------
-- TMVT
-- id,nbc,nbt,grp,xid,usr,xoid,own_src,own_dst,qtt,nat,ack,exhausted,order_created,created
--------------------------------------------------------------------------------
create table tmvt (
	id serial UNIQUE not NULL,
	type dtypeorder not NULL,
	json text, -- can be NULL
	nbc int not NULL, -- Number of mvts in the exchange
	nbt int not NULL, -- Number of mvts in the transaction
	grp int, -- References the first mvt of an exchange.
	-- can be NULL
	xid int not NULL,
	usr text,
	xoid int not NULL,
	own_src text not NULL, 
	own_dst text not NULL,
	qtt dquantity not NULL,
	nat text not NULL,
	ack	boolean default false,
	exhausted boolean default false,
	refused int default 0,
	order_created timestamp not NULL,
	created	timestamp not NULL 
	CHECK (
		(nbc = 1 AND own_src = own_dst)
	OR 	(nbc !=1) -- ( AND own_src != own_dst)
	),
	CONSTRAINT ctmvt_grp FOREIGN KEY (grp) references tmvt(id) ON UPDATE CASCADE
);
SELECT _grant_read('tmvt');

comment on table tmvt is 'Records a ownership changes';
comment on column tmvt.nbc is 'number of movements of the exchange';
comment on column tmvt.nbt is 'number of movements of the transaction';
comment on column tmvt.grp is 'references the first movement of the exchange';
comment on column tmvt.xid is 'references the order.id';
comment on column tmvt.xoid is 'references the order.oid';
comment on column tmvt.own_src is 'owner provider';
comment on column tmvt.own_dst is 'owner receiver';
comment on column tmvt.qtt is 'quantity of the value moved';
comment on column tmvt.nat is 'quality of the value moved';

alter sequence tmvt_id_seq owned by tmvt.id;
create index tmvt_grp_idx on tmvt(grp);
create index tmvt_nat_idx on tmvt(nat);
create index tmvt_own_src_idx on tmvt(own_src);
create index tmvt_own_dst_idx on tmvt(own_dst);

SELECT fifo_init('tmvt');


--------------------------------------------------------------------------------
-- STACK
--------------------------------------------------------------------------------
create table tstack ( 
    id serial UNIQUE not NULL,
    usr text NOT NULL,
    own text NOT NULL,
    oid int, -- can be NULL
    type dtypeorder NOT NULL,
    qua_requ text NOT NULL ,
    qtt_requ dquantity NOT NULL,
    qua_prov text NOT NULL ,
    qtt_prov dquantity NOT NULL,
    qtt dquantity NOT NULL,
    created timestamp not NULL,   
    PRIMARY KEY (id)
);

comment on table tstack is 'Records a stack of orders';
comment on column tstack.id is 'id of this order';
comment on column tstack.oid is 'id of a parent order';
comment on column tstack.own is 'owner of this order';
comment on column tstack.type is 'type of order';
comment on column tstack.qua_requ is 'quality required';
comment on column tstack.qtt_requ is 'quantity required';
comment on column tstack.qua_prov is 'quality provided';
comment on column tstack.qtt_prov is 'quantity provided';

alter sequence tstack_id_seq owned by tstack.id;

SELECT _grant_read('tstack');
SELECT fifo_init('tstack');

--------------------------------------------------------------------------------
CREATE TYPE yressubmit AS (
    id int,
    diag int
);
--------------------------------------------------------------------------------
-- order submission  
/*
returns (id,0) or (0,diag) with diag:
	-1 _qua_prov = _qua_requ
	-2 _qtt_prov <=0 or _qtt_requ <=0
*/
--------------------------------------------------------------------------------
CREATE FUNCTION 
	fsubmitorder(_type dtypeorder,_own text,_oid int,_qua_requ text,_qtt_requ int8,_qua_prov text,_qtt_prov int8)
	RETURNS yressubmit AS $$
DECLARE
	_r			yressubmit%rowtype;	
BEGIN
	_r := fsubmitorder(_type,_own,_oid,_qua_requ,_qtt_requ,_qua_prov,_qtt_prov,_qtt_prov);
	RETURN _r;
END; 
$$ LANGUAGE PLPGSQL SECURITY DEFINER;
GRANT EXECUTE ON FUNCTION  fsubmitorder(dtypeorder,text,int,text,int8,text,int8) TO role_co;
--------------------------------------------------------------------------------
CREATE FUNCTION 
	fsubmitorder(_type dtypeorder,_own text,_oid int,_qua_requ text,_qtt_requ int8,_qua_prov text,_qtt_prov int8,_qtt int8)
	RETURNS yressubmit AS $$	
DECLARE
	_t			tstack%rowtype;
	_r			yressubmit%rowtype;
	_o			int;
	_tid		int;
BEGIN
	_r.id := 0;
	_r.diag := 0;
	IF(_qua_prov = _qua_requ) THEN
		_r.diag := -1;
		RETURN _r;
	END IF;
	
	IF(_qtt_requ <=0 OR _qtt_prov <= 0) THEN
		_r.diag := -2;
		RETURN _r;
	END IF;	
	
	INSERT INTO tstack(usr,own,oid,type,qua_requ,qtt_requ,qua_prov,qtt_prov,qtt,created)
	VALUES (current_user,_own,_oid,_type,_qua_requ,_qtt_requ,_qua_prov,_qtt_prov,_qtt,statement_timestamp()) RETURNING * into _t;
	_r.id := _t.id;
	RETURN _r;
END; 
$$ LANGUAGE PLPGSQL SECURITY DEFINER;
GRANT EXECUTE ON FUNCTION  fsubmitorder(dtypeorder,text,int,text,int8,text,int8,int8) TO role_co;	


--------------------------------------------------------------------------------
/* for an order O creates a temporary table _tmp of objects.
Each object represents a chain of orders - a flows - going to O. 

The table has columns
	debut 	the first order of the path
	path	the path
	fin		the end of the path (O)
	depth	the depth of exploration
	cycle	a boolean true when the path contains the new order

The number of paths fetched is limited to MAXPATHFETCHED
Among those objects representing chains of orders, 
only those making a potential exchange (draft) are recorded.
*/
--------------------------------------------------------------------------------
/*
CREATE VIEW vorderinsert AS
	SELECT id,yorder_get(id,own,nr,qtt_requ,np,qtt_prov,qtt) as ord,np,nr
	FROM torder ORDER BY ((qtt_prov::double precision)/(qtt_requ::double precision)) DESC; */
--------------------------------------------------------------------------------
CREATE FUNCTION fcreate_tmp(_ord yorder) RETURNS int AS $$
DECLARE 
	_MAXPATHFETCHED	 int := fgetconst('MAXPATHFETCHED');  
	_MAXCYCLE 	int := fgetconst('MAXCYCLE');
	_cnt int;
BEGIN
/* the statement LIMIT would not avoid deep exploration if the condition
was specified  on Z in the search_backward WHERE condition */
	CREATE TEMPORARY TABLE _tmp ON COMMIT DROP AS (
		SELECT yflow_finish(Z.debut,Z.path,Z.fin) as cycle FROM (
			WITH RECURSIVE search_backward(debut,path,fin,depth,cycle) AS(
					SELECT _ord,yflow_init(_ord),
						_ord,1,false 
					-- FROM torder WHERE (ord).id= _ordid
				UNION ALL
					SELECT X.ord,yflow_grow(X.ord,Y.debut,Y.path),
						Y.fin,Y.depth+1,yflow_contains_oid((X.ord).oid,Y.path)
					FROM torder X,search_backward Y
					WHERE (X.ord).qua_prov=(Y.debut).qua_requ 
						AND yflow_match(X.ord,Y.debut) 
						AND Y.depth < _MAXCYCLE 
						AND NOT cycle 
						AND NOT yflow_contains_oid((X.ord).oid,Y.path) 
			) SELECT debut,path,fin from search_backward 
			LIMIT _MAXPATHFETCHED
		) Z WHERE (Z.fin).qua_prov=(Z.debut).qua_requ 
				AND yflow_match(Z.fin,Z.debut)
				AND yflow_is_draft(yflow_finish(Z.debut,Z.path,Z.fin))
	);
	RETURN 0;
	
END;
$$ LANGUAGE PLPGSQL;

--------------------------------------------------------------------------------
-- order unstacked and inserted into torder
/* if the referenced oid is found,
	the order is inserted, and the process is loached
else a movement is created
*/
--------------------------------------------------------------------------------
CREATE TYPE yresexec AS (
    first_mvt int, -- id of the first mvt
    nbc int, -- number of mvts
    mvts int[] -- list of id des mvts
);
--------------------------------------------------------------------------------
CREATE TYPE yresorder AS (
    ord yorder,
    qtt_requ int8,
    qtt_prov int8,
    qtt int8,
    err	int,
    json text
);
--------------------------------------------------------------------------------
CREATE FUNCTION fproducemvt() RETURNS yresorder AS $$
DECLARE
	_wid		int;
	_o			yorder;
	_or			yorder;
	_op			yorder; --torder%rowtype;
	_t			tstack%rowtype;
	_ro		    yresorder%rowtype;
	_fmvtids	int[];
	_first_mvt 	int;
	_err		int;

	--_flows		json[]:= ARRAY[]::json[];
	_cyclemax 	yflow;
	_cycle		yflow;
	_res	    int8[];
	_tid		int;
	_resx		yresexec%rowtype;
	_time_begin	timestamp;
	_qtt		int8;
	_mid		int;
	_cnt		int;
	_qua_prov	text; --hstore
	_qtt_prov	int8;
	_oid		int;
BEGIN

	lock table torder in share update exclusive mode;
	_ro.ord 		:= NULL;
	_ro.qtt_requ	:= 0;
	_ro.qtt_prov	:= 0;
	_ro.qtt			:= 0;
	_ro.err			:= 0;
	_ro.json		:= NULL;
	
	SELECT id INTO _tid FROM tstack ORDER BY id ASC LIMIT 1;
	IF(NOT FOUND) THEN
		RETURN _ro; -- the stack is empty
	END IF;

	DELETE FROM tstack WHERE id=_tid RETURNING * INTO STRICT _t;
	
	
	_wid := fgetowner(_t.own);

	IF NOT (_t.oid IS NULL) THEN

		SELECT (ord).type,(ord).id,(ord).own,(ord).oid,(ord).qtt_requ,(ord).qua_requ,(ord).qtt_prov,(ord).qua_prov,(ord).qtt 
			INTO _op.type,_op.id,_op.own,_op.oid,_op.qtt_requ,_op.qua_requ,_op.qtt_prov,_op.qua_prov,_op.qtt FROM torder WHERE (ord).id= _t.oid;

		
		IF (NOT FOUND) THEN -- the parent must exist
			_ro.json:= '{"error":"the parent order must exist"}';
			_ro.err := -1; END IF;
		IF (_op.own != _wid) THEN 
			_ro.json:= '{"error":"the parent must have the same owner"}';
			_ro.err := -2; END IF;
		IF (_op.oid != _op.id) THEN 
			_ro.json:= '{"error":"the parent must not have parent"}';
			_ro.err := -3; END IF;
			
		IF (_ro.err != 0) THEN
			INSERT INTO tmvt (	type,nbc,nbt,grp,xid,    usr,xoid, own_src,own_dst,
								qtt,nat,ack,exhausted,refused,order_created,created
							 ) 
				VALUES       (	_t.type,1,  1,NULL,_t.id,_t.usr,_t.oid,_t.own,_t.own,
								_t.qtt,_t.qua_prov,false,_ro.err,true,_t.created,statement_timestamp()
							 )
				RETURNING id INTO _mid;
			UPDATE tmvt SET grp = _mid WHERE id = _mid;
			_ro.ord := _op;
			RETURN _ro; -- the referenced order was not found in the book
		ELSE
			_qtt	:= _op.qtt;
			_t.qua_prov := _op.qua_prov; 
		END IF;
	ELSE 
		_t.oid 	:= _t.id;		
		_qtt  	:= _t.qtt;
	END IF;
	
	_o := ROW(_t.type,_t.id,_wid,_t.oid,_t.qtt_requ,_t.qua_requ,_t.qtt_prov,_t.qua_prov,_qtt)::yorder;
	
	IF(_t.type < 4) THEN -- the order is a barter
		INSERT INTO torder(usr,ord,created,updated) VALUES (_t.usr,_o,_t.created,NULL);
	ELSE
		_ro := fproducequote(_o);	
		RETURN _ro;
	END IF;
	
	_ro.ord      	:= _o;
	 
	_fmvtids := ARRAY[]::int[];
	
	_time_begin := clock_timestamp();
	
	_cnt := fcreate_tmp(_o);

	LOOP	
		-- BIG PB ON TEST_3
		SELECT yflow_max(cycle) INTO _cyclemax FROM _tmp WHERE yflow_is_draft(cycle);	
		IF(NOT yflow_is_draft(_cyclemax)) THEN
			EXIT; -- from LOOP
		END IF;

		_resx := fexecute_flow(_cyclemax,_oid);
		_fmvtids := _fmvtids || _resx.mvts;
		
		_res := yflow_qtts(_cyclemax);
		_ro.qtt_requ := _ro.qtt_requ + _res[1];
		_ro.qtt_prov := _ro.qtt_prov + _res[2];
	
		UPDATE _tmp SET cycle = yflow_reduce(cycle,_cyclemax); 
		DELETE FROM _tmp WHERE NOT yflow_is_draft(cycle);
	END LOOP;
	
	IF (	(_ro.qtt_requ != 0) AND ((_o.type & 3) = 1) -- ORDER_LIMIT
	AND	((_ro.qtt_prov::double precision)	/(_ro.qtt_requ::double precision)) > 
		((_o.qtt_prov::double precision)	/(_o.qtt_requ::double precision))
	) THEN
		RAISE EXCEPTION 'Omega of the flows obtained is not limited by the order limit' USING ERRCODE='YA003';
	END IF;
	_ro.qtt := _ro.qtt_prov;
	-- set the number of movements in this transaction
	UPDATE tmvt SET nbt= array_length(_fmvtids,1) WHERE id = ANY (_fmvtids);
	
	RETURN _ro;

END; 
$$ LANGUAGE PLPGSQL SECURITY DEFINER;
GRANT EXECUTE ON FUNCTION  fproducemvt() TO role_bo;

--------------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION femptystack() RETURNS int AS $$
DECLARE
	_t tstack%rowtype;
	_res yresorder%rowtype;
	_cnt int := 0;
BEGIN
	LOOP
		SELECT * INTO _t FROM tstack ORDER BY created ASC LIMIT 1;
		EXIT WHEN NOT FOUND; 
		_cnt := _cnt +1;
		
		_res := fproducemvt();
		DROP TABLE _tmp;
/*		
		IF((_cnt % 100) =0) THEN
			CHECKPOINT;
		END IF;
*/	
	END LOOP;
	RETURN _cnt;
END;
$$ LANGUAGE PLPGSQL;
GRANT EXECUTE ON FUNCTION  femptystack() TO role_co;

--------------------------------------------------------------------------------
/* fexecute_flow used for a barter
from a flow representing a draft, for each order:
	inserts a new movement
	updates the order
*/
--------------------------------------------------------------------------------

CREATE FUNCTION fexecute_flow(_flw yflow,_oid int) RETURNS  yresexec AS $$
DECLARE
	_i			int;
	_next_i		int;
	_nbcommit	int;
	
	_first_mvt  int;
	_exhausted	boolean;
	_cntExhausted int;
	_mvt_id		int;

	_cnt 		int;
	_resx		yresexec%rowtype;
	_qtt		int8;
	_flowr		int8;
	_qttmin		int8;
	_qttmax		int8;
	_o			yorder;
	_usr		text;
	_own		text;
	_ownnext	text;
	_idownnext	int;
	_or			torder%rowtype;
	_mat		int8[][];
BEGIN
	_nbcommit := yflow_dim(_flw);
	
	-- sanity check
	IF( _nbcommit <2 ) THEN
		RAISE EXCEPTION 'the flow should be draft:_nbcommit = %',_nbcommit 
			USING ERRCODE='YA003';
	END IF;
	-- RAISE NOTICE 'flow of % commits',_nbcommit;
	
	--lock table torder in share row exclusive mode;
	lock table torder in share update exclusive mode;
	
	_first_mvt := NULL;
	_exhausted := false;
	_resx.nbc := _nbcommit;	
	_resx.mvts := ARRAY[]::int[];
	_mat := yflow_to_matrix(_flw);
	
	_i := _nbcommit;
	FOR _next_i IN 1 .. _nbcommit LOOP
		------------------------------------------------------------------------
		_o.id   := _mat[_i][1];
		_o.own	:= _mat[_i][2];
		_o.oid	:= _mat[_i][3];
		_o.qtt  := _mat[_i][6];
		_flowr:= _mat[_i][7]; 
		
		_idownnext := _mat[_next_i][2];	
		
		-- sanity check
		SELECT count(*),min((ord).qtt),max((ord).qtt) INTO _cnt,_qttmin,_qttmax 
			FROM torder WHERE (ord).oid = _o.oid;
			
		IF(_cnt = 0) THEN
			RAISE EXCEPTION 'the stock % expected does not exist',_o.oid  USING ERRCODE='YU002';
		END IF;
		
		IF( _qttmin != _qttmax ) THEN
			RAISE EXCEPTION 'the value of stock % is not the same value for all orders',_o.oid  USING ERRCODE='YU002';
		END IF;
		
		_cntExhausted := 0;
		IF( _qttmin < _flowr ) THEN
			RAISE EXCEPTION 'the stock % is smaller than the flow (% < %)',_o.oid,_qttmin,_flowr  USING ERRCODE='YU002';
		ELSIF (_qttmin = _flowr) THEN
			_cntExhausted := _cnt;
		END IF;
			
		-- update all stocks of the order book
		UPDATE torder SET ord.qtt = (ord).qtt - _flowr ,updated = statement_timestamp()
			WHERE (ord).oid = _o.oid; 
		GET DIAGNOSTICS _cnt = ROW_COUNT;
		IF(_cnt = 0) THEN
			RAISE EXCEPTION 'no orders with the stock % exist',_o.oid  USING ERRCODE='YU002';
		END IF;
			

		SELECT * INTO _or FROM torder WHERE (ord).oid = _o.oid LIMIT 1;

		SELECT name INTO STRICT _ownnext 	FROM towner WHERE id=_idownnext;
		SELECT name INTO STRICT _own 		FROM towner WHERE id=_o.own;
		
		INSERT INTO tmvt (type,json,nbc,nbt,grp,
						xid,usr,xoid,own_src,own_dst,qtt,nat,
						ack,exhausted,refused,order_created,created) 
			VALUES((_or.ord).type,NULL,_nbcommit,1,_first_mvt,
						_o.id,_or.usr,_o.oid,_own,_ownnext,_flowr,(_or.ord).qua_prov,
						false,_exhausted,0,_or.created,statement_timestamp())
			RETURNING id INTO _mvt_id;
			
		IF(_first_mvt IS NULL) THEN
			_first_mvt := _mvt_id;
			_resx.first_mvt := _mvt_id;
			UPDATE tmvt SET grp = _first_mvt WHERE id = _first_mvt;
		END IF;
		_resx.mvts := array_append(_resx.mvts,_mvt_id);

		IF( _cntExhausted > 0) THEN
			DELETE FROM torder WHERE (ord).oid=_o.oid AND (ord).qtt = 0 ;
			GET DIAGNOSTICS _cnt = ROW_COUNT;
			IF(_cnt != _cntExhausted) THEN
				RAISE EXCEPTION 'the stock % is not exhasted',_o.oid  USING ERRCODE='YU002';
			END IF;
			_exhausted := true;
		END IF;

		_i := _next_i;
		------------------------------------------------------------------------
	END LOOP;

	IF( NOT _exhausted ) THEN
		--  some order should be exhausted 
		RAISE EXCEPTION 'the cycle should exhaust some order' 
			USING ERRCODE='YA003';
	END IF;
	RETURN _resx;
END;
$$ LANGUAGE PLPGSQL;

--------------------------------------------------------------------------------
CREATE FUNCTION ackmvt() RETURNS int AS $$
DECLARE
	_cnt	int;
	_m  tmvt%rowtype;
BEGIN
	SELECT * INTO _m FROM tmvt WHERE usr=current_user AND ack=false ORDER BY id ASC LIMIT 1;
	IF(NOT FOUND) THEN
		RETURN 0;
	END IF;
	UPDATE tmvt SET ack=true WHERE id=_m.id;
	SELECT count(*) INTO STRICT _cnt FROM tmvt WHERE grp=_m.grp AND ack=false;
	IF(_cnt = 0) THEN
		DELETE FROM tmvt where grp=_m.grp;
	END IF;
	RETURN 1;

END; 
$$ LANGUAGE PLPGSQL SECURITY DEFINER;
GRANT EXECUTE ON FUNCTION  ackmvt() TO role_co;

-- \i sql/quote.sql
\i sql/stat.sql




