/* packet-sadmind.c
 * Stubs for the Solstice admin daemon RPC service
 *
 * Guy Harris <guy@alum.mit.edu>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "packet-rpc.h"

void proto_register_sadmind(void);
void proto_reg_handoff_sadmind(void);

static int proto_sadmind;
static int hf_sadmind_procedure_v1;
static int hf_sadmind_procedure_v2;
static int hf_sadmind_procedure_v3;

static int ett_sadmind;

#define SADMIND_PROGRAM	100232

#define SADMINDPROC_NULL		0

/* proc number, "proc name", dissect_request, dissect_reply */
static const vsff sadmind1_proc[] = {
	{ SADMINDPROC_NULL,	"NULL",
		dissect_rpc_void,	dissect_rpc_void },
	{ 0,	NULL,	NULL,	NULL }
};
static const value_string sadmind1_proc_vals[] = {
	{ SADMINDPROC_NULL,	"NULL" },
	{ 0,	NULL }
};

static const vsff sadmind2_proc[] = {
	{ SADMINDPROC_NULL,	"NULL",
		dissect_rpc_void,	dissect_rpc_void },
	{ 0,	NULL,	NULL,	NULL }
};
static const value_string sadmind2_proc_vals[] = {
	{ SADMINDPROC_NULL,	"NULL" },
	{ 0,	NULL }
};

static const vsff sadmind3_proc[] = {
	{ SADMINDPROC_NULL,	"NULL",
		dissect_rpc_void,	dissect_rpc_void },
	{ 0,	NULL,	NULL,	NULL }
};
static const value_string sadmind3_proc_vals[] = {
	{ SADMINDPROC_NULL,	"NULL" },
	{ 0,	NULL }
};

static const rpc_prog_vers_info sadmind_vers_info[] = {
	{ 1, sadmind1_proc, &hf_sadmind_procedure_v1 },
	{ 2, sadmind2_proc, &hf_sadmind_procedure_v2 },
	{ 3, sadmind3_proc, &hf_sadmind_procedure_v3 },
};

void
proto_register_sadmind(void)
{
	static hf_register_info hf[] = {
		{ &hf_sadmind_procedure_v1, {
			"V1 Procedure", "sadmind.procedure_v1", FT_UINT32, BASE_DEC,
			VALS(sadmind1_proc_vals), 0, NULL, HFILL }},
		{ &hf_sadmind_procedure_v2, {
			"V2 Procedure", "sadmind.procedure_v2", FT_UINT32, BASE_DEC,
			VALS(sadmind2_proc_vals), 0, NULL, HFILL }},
		{ &hf_sadmind_procedure_v3, {
			"V3 Procedure", "sadmind.procedure_v3", FT_UINT32, BASE_DEC,
			VALS(sadmind3_proc_vals), 0, NULL, HFILL }}
	};

	static int *ett[] = {
		&ett_sadmind,
	};

	proto_sadmind = proto_register_protocol("SADMIND", "SADMIND", "sadmind");
	proto_register_field_array(proto_sadmind, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_sadmind(void)
{
	/* Register the protocol as RPC */
	rpc_init_prog(proto_sadmind, SADMIND_PROGRAM, ett_sadmind,
	    G_N_ELEMENTS(sadmind_vers_info), sadmind_vers_info);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
