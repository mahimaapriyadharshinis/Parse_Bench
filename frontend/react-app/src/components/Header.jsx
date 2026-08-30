export default function Header() {
  return (
    <header>
      <div className="brand">
        <div className="mark">
          <svg viewBox="0 0 24 24" fill="none">
            <path
              d="M12 3v6M12 9 6 15M12 9l6 6M6 15v4a1 1 0 0 0 1 1h2M18 15v4a1 1 0 0 0-1 1h-2"
              stroke="currentColor"
              strokeWidth="1.7"
              strokeLinecap="round"
              strokeLinejoin="round"
            />
          </svg>
        </div>
        <div>
          <h1>Parse&nbsp;Bench</h1>
          <div className="tag">Grammar-aware syntax analyzer &middot; LL(1) recursive descent</div>
        </div>
      </div>
    </header>
  );
}
