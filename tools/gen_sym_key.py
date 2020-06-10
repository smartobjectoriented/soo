#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =========================================================================== #
# 5G prolive project - REDS Institute, HEIG-VD, Yverdon-les-Bains (CH) - 2020 s#
# =========================================================================== #
""" Generation of AES 256 symmetric keys

Generation of AES 256 symmetric keys.
The result is a header file which can be used in ASF TA src tree
   * 'asf_communication_key.h' for communication flow
   * 'asf_injection_key.h' for the injection flow
"""
# =========================================================================== #
__author__ = "Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch"
# =========================================================================== #

import subprocess
import argparse
import shutil
import os

ASF_TA_PATH = os.path.join(os.getcwd(), '..', 'optee_ta', 'asf')

print(ASF_TA_PATH)

class KeyFileInfo(object):
	"""docstring for ClassName"""
	def __init__(self, file_name, macro_name):
		self.file_name = file_name
		self.macro_name = macro_name

com_key_info    = KeyFileInfo('asf_communication_key.h', 'ASF_COM_KEY')
inject_key_info = KeyFileInfo('asf_injection_key.h',     'ASF_INJECT_KEY')


def gen_key():

	command =['openssl', 'rand', '-base64', '32']
	# command =['openssl', 'rand', '-hex', '32']
	proc = subprocess.Popen(command, stdout=subprocess.PIPE)
	results = proc.stdout.read()
	proc.wait()

	return results.decode('utf-8')


def gen_header(key, macro_name):
	return """
// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
 */

/* File generated by '%(program)s' tool */

#define %(macro)s		"%(key)s"

""" % { 'program': __file__, 'macro':  macro_name, 'key' : key[:-1]}


def create_header_file(file_name, header):
	f = open(file_name, "w")
	f.write(header)
	f.close()



def parse_args():
	parser = argparse.ArgumentParser(description='AES Key generation')

	parser.add_argument('-c', '--communication', dest='communication', action="store_true", help='Communication key generation')
	parser.add_argument('-i', '--injection', dest='injection', action="store_true", help='Injection key generation')

	return parser.parse_args()

def header_generation(file_name, macro_name):
	key = gen_key()
	header = gen_header(key, macro_name)
	create_header_file(file_name, header)

def main():
	args = parse_args();

	if (not args.communication) and (not args.injection):
		gen_all_keys = True

	if args.communication or gen_all_keys:
		header_generation(com_key_info.file_name, com_key_info.macro_name)
		shutil.copy(com_key_info.file_name, ASF_TA_PATH)

	if args.injection or gen_all_keys:
		header_generation(inject_key_info.file_name, inject_key_info.macro_name)
		shutil.copy(inject_key_info.file_name, ASF_TA_PATH)

if __name__ == "__main__":
	main()