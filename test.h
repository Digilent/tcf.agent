/*******************************************************************************
 * Copyright (c) 2007, 2008 Wind River Systems, Inc. and others.
 * All rights reserved. This program and the accompanying materials 
 * are made available under the terms of the Eclipse Public License v1.0 
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 * The Eclipse Public License is available at
 * http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *  
 * Contributors:
 *     Wind River Systems - initial API and implementation
 *******************************************************************************/

/*
 * Agent self-testing service.
 */

#ifndef D_test
#define D_test

#include "context.h"
#include "symbols.h"

#if SERVICE_RunControl
extern void test_proc(void);
extern int run_test_process(ContextAttachCallBack * done, void * data);
extern int find_test_symbol(Context * ctx, char * name, Symbol * sym, void ** addr);
#endif

#endif
