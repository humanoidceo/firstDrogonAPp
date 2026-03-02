/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      fontFamily: {
        sans: ["Space Grotesk", "system-ui", "sans-serif"],
        mono: ["IBM Plex Mono", "monospace"]
      },
      boxShadow: {
        panel: "0 30px 70px -25px rgba(20, 33, 61, 0.45)"
      }
    }
  },
  plugins: []
};
