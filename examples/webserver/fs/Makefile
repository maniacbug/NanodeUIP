OUTDIR = ..
INFILES = $(wildcard *.html) $(wildcard *.shtml) $(wildcard *.txt) $(wildcard *.js)
XXDDIR = xxd
XXDFILES = $(addprefix $(XXDDIR)/, $(addsuffix .cpp, $(INFILES)))
OUTFILES = $(addprefix $(OUTDIR)/, $(addsuffix .cpp, $(subst .,_,$(INFILES))))
ALLFILES = $(OUTDIR)/fs.cpp $(OUTDIR)/files.h $(OUTDIR)/dir.h

all: $(ALLFILES)

clean:
	rm -f $(ALLFILES) $(XXDFILES)

$(XXDDIR)/%.cpp : %
	xxd -i $< | gawk -f ../xxd.awk > $@

$(OUTDIR)/files.h : $(XXDFILES)
	gawk -f ../files.awk $^ > $@

$(OUTDIR)/dir.h : $(XXDFILES)
	gawk -f ../dir.awk $^ > $@

$(OUTDIR)/fs.cpp : $(XXDFILES)
	cat $(XXDFILES) > $@
