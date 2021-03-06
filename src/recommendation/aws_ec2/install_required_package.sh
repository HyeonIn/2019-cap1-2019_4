#!/bin/bash

sudo apt-get update

# python install
sudo apt-get install python

# pip install
sudo apt-get install python-pip

# install python-mysqldb
sudo apt-get install python-mysqldb

# it is not certain to install below package. if connection between DB and ec2 is failed, run this command. 
# sudo pip install mysql-connector-python-rf

#install pandas lib
pip install pandas

#install xgboost
pip install xgboost 

#install the latest Boto 3 release via pip
pip install boto3
