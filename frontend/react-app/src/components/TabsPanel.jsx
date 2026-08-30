import { useState } from "react";
import ParseTreeView from "./ParseTreeView.jsx";
import TokensView from "./TokensView.jsx";
import GrammarView from "./GrammarView.jsx";
import ErrorsView from "./ErrorsView.jsx";
import Walkthrough from "./Walkthrough.jsx";

const TABS = [
  { key: "walkthrough", label: "Step by step" },
  { key: "tree", label: "Parse tree" },
  { key: "tokens", label: "Tokens" },
  { key: "grammar", label: "Grammar & LL(1)" },
  { key: "errors", label: "Errors" },
];

export default function TabsPanel({ tree, tokens, errors, trace }) {
  const [active, setActive] = useState("walkthrough");

  return (
    <div className="panel">
      <div className="tabbar" role="tablist">
        {TABS.map((t) => (
          <button
            key={t.key}
            className={`tabbtn${active === t.key ? " active" : ""}`}
            role="tab"
            onClick={() => setActive(t.key)}
          >
            {t.label}
          </button>
        ))}
      </div>

      <div className="tabpane" hidden={active !== "walkthrough"}>
        <Walkthrough key={trace} tree={tree} tokens={tokens} trace={trace} />
      </div>
      <div className="tabpane" hidden={active !== "tree"}>
        <ParseTreeView tree={tree} />
      </div>
      <div className="tabpane" hidden={active !== "tokens"}>
        <TokensView tokens={tokens} />
      </div>
      <div className="tabpane" hidden={active !== "grammar"}>
        <GrammarView />
      </div>
      <div className="tabpane" hidden={active !== "errors"}>
        <ErrorsView errors={errors} />
      </div>
    </div>
  );
}
