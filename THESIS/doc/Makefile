
all: doc

doc: doc.tex doc.bib
	pdflatex doc.tex
	bibtex doc
	pdflatex doc.tex
	pdflatex doc.tex

clean:
	rm -f *.aux *.bbl *.blg *.log *.out *.toc *.lol *.lot *.lof
