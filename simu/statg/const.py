#!/usr/bin/python
# -*- coding: utf8 -*-
DB_NAME='market'
DB_USER='olivier'
DB_PWD=''
DB_HOST='localhost'
DB_PORT=5432

PATH_SRC="/home/olivier/Bureau/ob92/src"

MAX_OWNER=100 # maximum number of owners
MAX_USER=10 # user0,user1,...
MAX_QLT=100  # maximum number  of qualities
FLOW_MAX_DIM=64
"""
def crible(dim):
	a=(dim+1)*[True]
	a[0],a[1]=False,False
	for i in range(2,dim+1):
		for j in range(i+1,dim+1):
			if(a[j] and (j%i ==0)):
				a[j] = False
	for i in range(dim+1):
		if (a[i]): print i,
	return
	
crible(1000) = ...	
8693 8699 8707 8713 8719 8731 8737 8741 8747 8753 8761 8779 8783 8803 8807 8819 8821 8831 8837 8839 8849 8861 8863 8867 8887 8893 8923 8929 8933 8941 8951 8963 8969 8971 8999 9001 9007 9011 9013 9029 9041 9043 9049 9059 9067 9091 9103 9109 9127 9133 9137 9151 9157 9161 9173 9181 9187 9199 9203 9209 9221 9227 9239 9241 9257 9277 9281 9283 9293 9311 9319 9323 9337 9341 9343 9349 9371 9377 9391 9397 9403 9413 9419 9421 9431 9433 9437 9439 9461 9463 9467 9473 9479 9491 9497 9511 9521 9533 9539 9547 9551 9587 9601 9613 9619 9623 9629 9631 9643 9649 9661 9677 9679 9689 9697 9719 9721 9733 9739 9743 9749 9767 9769 9781 9787 9791 9803 9811 9817 9829 9833 9839 9851 9857 9859 9871 9883 9887 9901 9907 9923 9929 9931 9941 9949 9967 9973

"""