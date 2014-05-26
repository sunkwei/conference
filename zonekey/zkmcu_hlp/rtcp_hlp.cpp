#include <ortp/ortp.h>
#include "rtcp_hlp.h"
#include <vector>

namespace 
{
struct ReportBlock
{
	unsigned int jitter, frac_lost, cum_lost, ssrc;
};

struct Pkg
{
	std::vector<ReportBlock*> report_blocks;
	std::vector<ReportBlock*>::iterator it;
	int type;	// 1 sr, 2 rr
};

struct CTX
{
	std::vector<Pkg*> pkgs;
	std::vector<Pkg*>::iterator it;
};
};

static void parse_report_block(mblk_t *e, Pkg *pkg)
{
	const rtcp_common_header_t *header = rtcp_get_common_header(e);
	for (int i = 0; i < header->rc; i++) {
		ReportBlock *r = new ReportBlock;
		const report_block_t *rb = rtcp_RR_get_report_block(e, i);
		r->jitter = report_block_get_interarrival_jitter(rb);
		r->cum_lost = report_block_get_cum_packet_loss(rb);
		r->frac_lost = report_block_get_fraction_lost(rb);
		r->ssrc = report_block_get_ssrc(rb);
		pkg->report_blocks.push_back(r);
	}
	pkg->it = pkg->report_blocks.begin();
}

rtcp_hlp_t *rtcp_hlp_open(OrtpEvent *evt)
{
	CTX *ctx = new CTX;
	do {
		if (rtcp_is_SR(evt)) {
			Pkg *pkg = new Pkg;
			pkg->type = 1;
			parse_report_block(evt, pkg);
			ctx->pkgs.push_back(pkg);
		}
		else if (rtcp_is_RR(evt)) {
			Pkg *pkg = new Pkg;
			pkg->type = 2;
			parse_report_block(evt, pkg);
			ctx->pkgs.push_back(pkg);
		}
	} while (rtcp_next_packet(evt));

	ctx->it = ctx->pkgs.begin();
	return (rtcp_hlp_t*)ctx;
}

void rtcp_hlp_close(rtcp_hlp_t *ctx)
{
	CTX *c = (CTX*)ctx;
	for (std::vector<Pkg*>::iterator itp = c->pkgs.begin(); itp != c->pkgs.end(); ++itp) {
		Pkg *p = *itp;

		for (std::vector<ReportBlock*>::iterator itr = p->report_blocks.begin(); itr != p->report_blocks.end(); ++itr) {
			ReportBlock *r = *itr;
			delete r;
		}

		delete p;
	}
	delete c;
}

rtcp_hlp_pkg_t *rtcp_hlp_pkg_next(rtcp_hlp_t *ctx)
{
	CTX *c = (CTX*)ctx;
	if (c->it != c->pkgs.end()) {
		Pkg *p = *(c->it);
		++(c->it);
		return (rtcp_hlp_pkg_t*)p;
	}
	return 0;
}

int rtcp_hlp_pkg_is_sr(rtcp_hlp_pkg_t *pkg)
{
	Pkg *p = (Pkg*)pkg;
	if (p->type == 1)
		return -1;
	else
		return 0;
}

int rtcp_hlp_pkg_is_rr(rtcp_hlp_pkg_t *pkg)
{
	Pkg *p = (Pkg*)pkg;
	if (p->type == 2)
		return -1;
	else
		return 0;
}

rtcp_hlp_report_block_t *rtcp_hlp_report_block_next(rtcp_hlp_pkg_t *pkg)
{
	Pkg *p = (Pkg*)pkg;
	if (p->it != p->report_blocks.end()) {
		ReportBlock *r = *(p->it);
		++(p->it);
		return (rtcp_hlp_report_block_t*)p;
	}
	return 0;
}

unsigned int rtcp_hlp_report_block_ssrc(rtcp_hlp_report_block_t *rb)
{
	return ((ReportBlock*)rb)->ssrc;
}

unsigned int rtcp_hlp_report_block_fraction_lost(rtcp_hlp_report_block_t *rb)
{
	return ((ReportBlock*)rb)->frac_lost;
}

unsigned int rtcp_hlp_report_block_cum_lost(rtcp_hlp_report_block_t *rb)
{
	return ((ReportBlock*)rb)->cum_lost;
}

unsigned int rtcp_hlp_report_block_jitter(rtcp_hlp_report_block_t *rb)
{
	return ((ReportBlock*)rb)->jitter;
}
