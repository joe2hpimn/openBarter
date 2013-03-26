#!/usr/bin/python
# -*- coding: utf8 -*-
import distrib

DB_NAME='liquid'
DB_USER='olivier'
DB_PWD=''
DB_HOST='localhost'
DB_PORT=5432

PATH_SRC="/home/olivier/Bureau/ob92/src"
PATH_DATA="/home/olivier/Bureau/ob92/simu/liquid/data"

MAX_TOWNER=10000 # maximum number of owners in towner
MAX_TORDER=1000000 # maximum size of the order book
MAX_TSTACK=1000
MAX_QLT=100  # maximum number  of qualities

#
QTT_PROV = 10000 # quantity provided

class Exec1:
    def __init__(self):
        self.NAME = "Y6P1024M128"
        # model
        self.MAXCYCLE=6
        self.MAXPATHFETCHED=1024
        self.MAXMVTPERTRANS=128


class Exec2:
    def __init__(self):
        self.NAME = "Y6P2048M128"
        # model
        self.MAXCYCLE=6
        self.MAXPATHFETCHED=2048
        self.MAXMVTPERTRANS=128
        

class Exec3:
    def __init__(self):
        self.NAME = "Y8P1024M128"
        # model
        self.MAXCYCLE=8
        self.MAXPATHFETCHED=1024
        self.MAXMVTPERTRANS=128


class Exec4:
    def __init__(self):
        self.NAME = "Y6P1024M256"
        # model
        self.MAXCYCLE=6
        self.MAXPATHFETCHED=1024
        self.MAXMVTPERTRANS=256
                       
class Basic100:
    def __init__(self):

        self.CONF_NAME='uni100'

        self.MAX_OWNER=min(100,MAX_TOWNER) # maximum number of owners
        self.MAX_QLT=100  # maximum number  of qualities

        """
        fonction de distribution des qualités
        """
        self.distribQlt = distrib.uniformQlt
        self.coupleQlt = distrib.couple

        # etendue des tests
        self.LIQ_PAS = 30
        self.LIQ_ITER = min(30,MAX_TORDER/self.LIQ_PAS)

                
class Basic1000:
    def __init__(self):

        self.CONF_NAME='uni1000'

        self.MAX_OWNER=min(100,MAX_TOWNER) # maximum number of owners
        self.MAX_QLT=1000  # maximum number  of qualities

        """
        fonction de distribution des qualités
        """
        self.distribQlt = distrib.uniformQlt
        self.coupleQlt = distrib.couple

        # etendue des tests
        self.LIQ_PAS = 30
        self.LIQ_ITER = min(30,MAX_TORDER/self.LIQ_PAS)
        
                
class Basic1000large:
    def __init__(self):

        self.CONF_NAME='UNI1000'

        self.MAX_OWNER=min(100,MAX_TOWNER) # maximum number of owners
        self.MAX_QLT=1000  # maximum number  of qualities

        """
        fonction de distribution des qualités
        """
        self.distribQlt = distrib.uniformQlt
        self.coupleQlt = distrib.couple

        # etendue des tests
        self.LIQ_PAS = 10000
        self.LIQ_ITER = min(30,MAX_TORDER/self.LIQ_PAS)
        

class Money100:
    def __init__(self):

        self.CONF_NAME='money100'

        self.MAX_OWNER=min(100,MAX_TOWNER) # maximum number of owners
        self.MAX_QLT=100  # maximum number  of qualities

        """
        fonction de distribution des qualités
        """
        self.distribQlt = distrib.uniformQlt
        self.coupleQlt = distrib.couple_money

        # etendue des tests
        self.LIQ_PAS = 30
        self.LIQ_ITER = min(30,MAX_TORDER/self.LIQ_PAS)

