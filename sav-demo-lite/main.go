package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"

	"github.com/Cq-zgclab/sav-demo-lite/internal"
)

var (
	port   = flag.Int("port", 8080, "HTTP server port")
	stream *internal.StreamServer
)

func main() {
	flag.Parse()
	
	// ASCII Art Banner
	printBanner()
	
	// Initialize stream server
	stream = internal.NewStreamServer()
	
	// Load SAV records from scenarios
	if err := stream.LoadRecords(); err != nil {
		log.Fatalf("❌ Failed to load records: %v", err)
	}
	
	// Start playback loop
	stream.Start()
	
	// Setup HTTP routes
	setupRoutes()
	
	// Start server
	addr := fmt.Sprintf(":%d", *port)
	log.Printf("🚀 SAV IPFIX Demo Server starting on http://localhost%s", addr)
	log.Printf("📊 Open http://localhost%s in your browser", addr)
	log.Println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
	
	if err := http.ListenAndServe(addr, nil); err != nil {
		log.Fatalf("❌ Server failed: %v", err)
	}
}

func setupRoutes() {
	// SSE stream endpoint
	http.HandleFunc("/api/stream/events", stream.SSEHandler)
	
	// Control API
	http.HandleFunc("/api/control/start", handleStart)
	http.HandleFunc("/api/control/pause", handlePause)
	http.HandleFunc("/api/control/resume", handleResume)
	http.HandleFunc("/api/control/reset", handleReset)
	http.HandleFunc("/api/control/speed", handleSpeed)
	http.HandleFunc("/api/status", handleStatus)
	
	// Static files
	http.Handle("/", http.FileServer(http.Dir("web")))
	
	log.Println("✅ Routes configured:")
	log.Println("   GET  /                      - Main dashboard")
	log.Println("   GET  /api/stream/events     - SSE event stream")
	log.Println("   POST /api/control/start     - Start playback")
	log.Println("   POST /api/control/pause     - Pause playback")
	log.Println("   POST /api/control/resume    - Resume playback")
	log.Println("   POST /api/control/reset     - Reset playback")
	log.Println("   POST /api/control/speed     - Set speed (0.5, 1, 2, 4)")
	log.Println("   GET  /api/status            - Get status")
}

// Control handlers
func handleStart(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	stream.Control("start")
	jsonResponse(w, map[string]string{"status": "ok", "action": "started"})
}

func handlePause(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	stream.Control("pause")
	jsonResponse(w, map[string]string{"status": "ok", "action": "paused"})
}

func handleResume(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	stream.Control("resume")
	jsonResponse(w, map[string]string{"status": "ok", "action": "resumed"})
}

func handleReset(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	stream.Control("reset")
	jsonResponse(w, map[string]string{"status": "ok", "action": "reset"})
}

func handleSpeed(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	
	speedStr := r.URL.Query().Get("value")
	if speedStr == "" {
		http.Error(w, "Missing speed value", http.StatusBadRequest)
		return
	}
	
	speed, err := strconv.ParseFloat(speedStr, 64)
	if err != nil {
		http.Error(w, "Invalid speed value", http.StatusBadRequest)
		return
	}
	
	stream.SetSpeed(internal.PlaybackSpeed(speed))
	jsonResponse(w, map[string]interface{}{
		"status": "ok",
		"speed":  speed,
	})
}

func handleStatus(w http.ResponseWriter, r *http.Request) {
	status := stream.GetStatus()
	jsonResponse(w, status)
}

func jsonResponse(w http.ResponseWriter, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(data)
}

func printBanner() {
	banner := `
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║      SAV IPFIX Real-Time Streaming Demo System           ║
║                                                           ║
║      Source Address Validation + IPFIX Telemetry         ║
║      RFC 7011 | RFC 6313 | draft-cao-opsawg-ipfix-sav    ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
`
	fmt.Fprintln(os.Stderr, banner)
}
