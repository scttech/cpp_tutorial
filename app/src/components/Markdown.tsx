import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";

export function Markdown({ children }: { children: string }) {
  return (
    <div className="md">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={{
          // a code block that scrolls sideways must be reachable from the keyboard
          pre: ({ node: _node, ...props }) => <pre tabIndex={0} {...props} />,
        }}
      >
        {children}
      </ReactMarkdown>
    </div>
  );
}
