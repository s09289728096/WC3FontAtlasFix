#pragma once
#include <windows.h>
struct RepairSites { unsigned abi; BYTE *uv,*clear,*uvBody,*atlasEntry; };
bool FindRepairSites(BYTE* image,RepairSites* output);
extern RepairSites g_sites;
