import { FormEvent, useState } from "react";

type AskApiResponse = {
  status?: string;
  answer?: string;
  message?: string;
  details?: string;
};

export default function App() {
  const [question, setQuestion] = useState("");
  const [answer, setAnswer] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  async function onSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!question.trim()) {
      setError("Please write a question first.");
      return;
    }

    setLoading(true);
    setError("");
    setAnswer("");

    try {
      const response = await fetch("/hugginfacechatter", {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({ question: question.trim() })
      });

      const data: AskApiResponse = await response.json();

      if (!response.ok || data.status !== "ok") {
        const failureReason = data.message || data.details || "Request failed";
        throw new Error(failureReason);
      }

      setAnswer(data.answer || "");
    } catch (requestError) {
      const message =
        requestError instanceof Error ? requestError.message : "Unexpected error";
      setError(message);
    } finally {
      setLoading(false);
    }
  }

  return (
    <main className="min-h-screen bg-[radial-gradient(circle_at_20%_20%,#f1dcb5_0%,transparent_30%),radial-gradient(circle_at_80%_0%,#9ed8d8_0%,transparent_35%),linear-gradient(145deg,#f7f2e9_0%,#efe8da_45%,#dde8f2_100%)] px-4 py-8 text-slate-900 sm:px-8">
      <section className="mx-auto w-full max-w-4xl rounded-3xl border border-white/60 bg-white/70 p-6 shadow-panel backdrop-blur-md sm:p-10">
        <div className="mb-8 flex flex-wrap items-start justify-between gap-4">
          <div>
            <p className="font-mono text-xs uppercase tracking-[0.28em] text-slate-600">
              Drogon + Ollama
            </p>
            <h1 className="mt-2 text-3xl font-semibold sm:text-5xl">Chatter Console</h1>
          </div>
          <p className="max-w-xs text-sm leading-relaxed text-slate-600">
            Ask a question and this UI will call your
            <code className="mx-1 rounded bg-slate-900 px-1.5 py-0.5 font-mono text-xs text-slate-100">
              /hugginfacechatter
            </code>
            endpoint.
          </p>
        </div>

        <form className="space-y-4" onSubmit={onSubmit}>
          <label className="block text-sm font-medium text-slate-700" htmlFor="question">
            Your question
          </label>
          <textarea
            id="question"
            className="min-h-36 w-full rounded-2xl border border-slate-300 bg-white/80 px-4 py-3 text-base leading-relaxed outline-none transition focus:border-slate-700 focus:ring-2 focus:ring-slate-300"
            placeholder="Explain how JWT refresh tokens work..."
            value={question}
            onChange={(event) => setQuestion(event.target.value)}
          />

          <div className="flex flex-wrap items-center gap-3">
            <button
              type="submit"
              className="rounded-xl bg-slate-900 px-5 py-2.5 text-sm font-semibold text-white transition hover:bg-slate-700 disabled:cursor-not-allowed disabled:opacity-60"
              disabled={loading}
            >
              {loading ? "Thinking..." : "Ask"}
            </button>
            <button
              type="button"
              className="rounded-xl border border-slate-400 px-5 py-2.5 text-sm font-semibold text-slate-700 transition hover:bg-slate-100"
              onClick={() => {
                setQuestion("");
                setAnswer("");
                setError("");
              }}
              disabled={loading}
            >
              Clear
            </button>
          </div>
        </form>

        <section className="mt-8">
          <h2 className="mb-3 text-sm font-semibold uppercase tracking-[0.2em] text-slate-600">
            Response
          </h2>
          <div className="min-h-44 rounded-2xl border border-slate-200 bg-white p-5">
            {error ? (
              <p className="font-mono text-sm text-red-600">{error}</p>
            ) : answer ? (
              <p className="whitespace-pre-wrap leading-relaxed text-slate-800">{answer}</p>
            ) : (
              <p className="text-sm text-slate-500">No answer yet.</p>
            )}
          </div>
        </section>
      </section>
    </main>
  );
}
