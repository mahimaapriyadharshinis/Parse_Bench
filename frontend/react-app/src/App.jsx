import { useState, useCallback } from "react";
import Header from "./components/Header.jsx";
import Hero from "./components/Hero.jsx";
import InputPanel from "./components/InputPanel.jsx";
import TabsPanel from "./components/TabsPanel.jsx";
import { parseTokenStreamText, TokenStreamFormatError } from "./lib/tokenStream.js";
import { Parser, countNodesAndDepth } from "./lib/parser.js";
import { EXAMPLES } from "./data/examples.js";

function runAnalysis(text) {
  const tokens = parseTokenStreamText(text);
  const parser = new Parser(tokens);
  const tree = parser.parse();
  const { count, maxDepth } = countNodesAndDepth(tree);
  return { tokens, tree, errors: parser.errors, trace: parser.trace, nodeCount: count, maxDepth };
}

const initial = runAnalysis(EXAMPLES.valid.text);

export default function App() {
  const [text, setText] = useState(EXAMPLES.valid.text);
  const [result, setResult] = useState(initial);
  const [formatError, setFormatError] = useState(null);

  const analyze = useCallback((source) => {
    try {
      setResult(runAnalysis(source ?? text));
      setFormatError(null);
    } catch (err) {
      if (!(err instanceof TokenStreamFormatError)) throw err;
      setFormatError(err.message);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [text]);

  const handleChange = (value) => {
    setText(value);
    try {
      setResult(runAnalysis(value));
      setFormatError(null);
    } catch (err) {
      if (!(err instanceof TokenStreamFormatError)) throw err;
      setFormatError(err.message);
    }
  };

  const realTokenCount = result.tokens.filter((t) => t.type !== "EOF").length;

  return (
    <div className="wrap">
      <Header />
      <Hero
        tokenCount={realTokenCount}
        nodeCount={result.nodeCount}
        maxDepth={result.maxDepth}
        errorCount={result.errors.length}
        hasTokens={realTokenCount > 0}
      />
      <div className="layout">
        <InputPanel value={text} onChange={handleChange} onAnalyze={() => analyze()} formatError={formatError} />
        <TabsPanel tree={result.tree} tokens={result.tokens} errors={result.errors} trace={result.trace} />
      </div>
      <footer>
        Recursive-descent LL(1) parser &middot; phrase-level + panic-mode error recovery &middot; runs
        entirely in this app
      </footer>
    </div>
  );
}
