/*******************************************************************************
 * Copyright (c) 2009 Philippe Proulx, École Polytechnique de Montréal
 *                    Michael Sills-Lavoie, École Polytechnique de Montréal
 * and others. All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 * The Eclipse Public License is available at
 * http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *     Philippe Proulx - initial plugins system
 *******************************************************************************/

/*
 * Plugins system.
 */

#ifndef D_plugins
#define D_plugins

#include "protocol.h"

#define _QUOTEME(x)     #x
#define QUOTE(x)      _QUOTEME(x)

#define PLUGINS_DEF_EXT     "so"        /* Default plugins' extension */

/*
 * Loads ALL plugins from the directory PATH_Plugins (from `config.h').
 */
int plugins_load(Protocol *, TCFBroadcastGroup *, TCFSuspendGroup *);

/*
 * Initializes a particular plugin according to its path.
 */
int plugin_init(const char *, Protocol *, TCFBroadcastGroup *, TCFSuspendGroup *);

/*
 * Destroys loaded plugins.
 */
int plugins_destroy(void);

#endif /* D_plugins */

