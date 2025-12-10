package internal

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"sync"
	"time"

	sav "github.com/Cq-zgclab/sav-ipfix-validator/go-implementation/pkg/sav"
)

// PlaybackSpeed represents the playback speed multiplier
type PlaybackSpeed float64

const (
	SpeedSlow   PlaybackSpeed = 0.5
	SpeedNormal PlaybackSpeed = 1.0
	SpeedFast   PlaybackSpeed = 2.0
	SpeedVeryFast PlaybackSpeed = 4.0
)

// StreamEvent represents an SSE event
type StreamEvent struct {
	Type string      `json:"type"`
	Data interface{} `json:"data"`
}

// RecordEvent represents a single SAV record event
type RecordEvent struct {
	RecordNumber     int                `json:"record_number"`
	TotalRecords     int                `json:"total_records"`
	Timestamp        string             `json:"timestamp"`
	RuleType         uint8              `json:"rule_type"`
	RuleTypeName     string             `json:"rule_type_name"`
	TargetType       uint8              `json:"target_type"`
	TargetTypeName   string             `json:"target_type_name"`
	PolicyAction     uint8              `json:"policy_action"`
	PolicyActionName string             `json:"policy_action_name"`
	Mappings         []MappingDetail    `json:"mappings"`
	PacketCount      uint64             `json:"packet_count,omitempty"`
	ByteCount        uint64             `json:"byte_count,omitempty"`
	Label            string             `json:"label"`
	ScenarioName     string             `json:"scenario_name"`
}

// MappingDetail represents a single interface-prefix mapping
type MappingDetail struct {
	InterfaceID  uint32 `json:"interface_id"`
	Prefix       string `json:"prefix"`
	PrefixLength uint8  `json:"prefix_length"`
	IsIPv6       bool   `json:"is_ipv6"`
}

// StreamServer manages SSE connections and playback
type StreamServer struct {
	records       []RecordEvent
	clients       map[chan StreamEvent]bool
	clientsMux    sync.RWMutex
	
	// Playback state
	isPlaying     bool
	isPaused      bool
	currentIndex  int
	speed         PlaybackSpeed
	stateMux      sync.RWMutex
	
	// Control channels
	controlChan   chan string
	stopChan      chan bool
}

// NewStreamServer creates a new stream server
func NewStreamServer() *StreamServer {
	return &StreamServer{
		records:     make([]RecordEvent, 0),
		clients:     make(map[chan StreamEvent]bool),
		speed:       SpeedNormal,
		controlChan: make(chan string, 10),
		stopChan:    make(chan bool),
	}
}

// LoadRecords loads SAV records from scenarios
func (s *StreamServer) LoadRecords() error {
	scenarios := []struct {
		name     string
		scenario sav.Scenario
	}{
		{"Spoofing-Attack-Detection", sav.GetScenario1()},
		{"Multi-Interface-Distribution", sav.GetScenario2()},
		{"Policy-Action-Effectiveness", sav.GetScenario3()},
	}
	
	recordNum := 0
	for _, sc := range scenarios {
		for _, record := range sc.scenario.Records {
			recordNum++
			
			// Convert mappings
			mappings := make([]MappingDetail, 0, len(record.Rules))
			for _, rule := range record.Rules {
				mappings = append(mappings, MappingDetail{
					InterfaceID:  rule.Interface,
					Prefix:       rule.Prefix.String(),
					PrefixLength: rule.PrefixLen,
					IsIPv6:       rule.IsIPv6,
				})
			}
			
			event := RecordEvent{
				RecordNumber:     recordNum,
				TotalRecords:     27, // We know total is 27
				Timestamp:        record.Timestamp.Format("2006-01-02 15:04:05"),
				RuleType:         record.RuleType,
				RuleTypeName:     getRuleTypeName(record.RuleType),
				TargetType:       record.TargetType,
				TargetTypeName:   getTargetTypeName(record.TargetType),
				PolicyAction:     record.Action,
				PolicyActionName: getPolicyActionName(record.Action),
				Mappings:         mappings,
				PacketCount:      record.PacketCount,
				ByteCount:        record.ByteCount,
				Label:            record.Label,
				ScenarioName:     sc.name,
			}
			
			s.records = append(s.records, event)
		}
	}
	
	log.Printf("✅ Loaded %d SAV IPFIX records from 3 scenarios", len(s.records))
	return nil
}

// AddClient adds a new SSE client
func (s *StreamServer) AddClient(client chan StreamEvent) {
	s.clientsMux.Lock()
	defer s.clientsMux.Unlock()
	s.clients[client] = true
	log.Printf("📡 New SSE client connected (total: %d)", len(s.clients))
}

// RemoveClient removes an SSE client
func (s *StreamServer) RemoveClient(client chan StreamEvent) {
	s.clientsMux.Lock()
	defer s.clientsMux.Unlock()
	delete(s.clients, client)
	close(client)
	log.Printf("📡 SSE client disconnected (total: %d)", len(s.clients))
}

// Broadcast sends an event to all connected clients
func (s *StreamServer) Broadcast(event StreamEvent) {
	s.clientsMux.RLock()
	defer s.clientsMux.RUnlock()
	
	for client := range s.clients {
		select {
		case client <- event:
		default:
			// Client buffer full, skip
		}
	}
}

// Start begins the playback loop
func (s *StreamServer) Start() {
	go s.playbackLoop()
}

// playbackLoop handles the main playback logic
func (s *StreamServer) playbackLoop() {
	for {
		select {
		case cmd := <-s.controlChan:
			s.handleControl(cmd)
			
		case <-s.stopChan:
			log.Println("🛑 Stopping playback loop")
			return
			
		default:
			s.stateMux.RLock()
			playing := s.isPlaying && !s.isPaused
			index := s.currentIndex
			speed := s.speed
			s.stateMux.RUnlock()
			
			if playing && index < len(s.records) {
				// Send current record
				record := s.records[index]
				s.Broadcast(StreamEvent{
					Type: "record",
					Data: record,
				})
				
				// Move to next
				s.stateMux.Lock()
				s.currentIndex++
				
				// Check if reached end
				if s.currentIndex >= len(s.records) {
					s.Broadcast(StreamEvent{
						Type: "completed",
						Data: map[string]interface{}{
							"message": "Playback completed. Click Reset to replay.",
						},
					})
					s.isPlaying = false
					s.currentIndex = 0 // Auto reset for loop
				}
				s.stateMux.Unlock()
				
				// Wait based on speed (base interval: 2 seconds)
				baseInterval := 2000 * time.Millisecond
				waitTime := time.Duration(float64(baseInterval) / float64(speed))
				time.Sleep(waitTime)
			} else {
				// Not playing, just wait
				time.Sleep(100 * time.Millisecond)
			}
		}
	}
}

// handleControl processes control commands
func (s *StreamServer) handleControl(cmd string) {
	s.stateMux.Lock()
	defer s.stateMux.Unlock()
	
	switch cmd {
	case "start":
		if !s.isPlaying {
			s.isPlaying = true
			s.isPaused = false
			log.Println("▶️  Playback started")
			s.Broadcast(StreamEvent{
				Type: "status",
				Data: map[string]interface{}{"status": "playing"},
			})
		}
		
	case "pause":
		if s.isPlaying && !s.isPaused {
			s.isPaused = true
			log.Println("⏸️  Playback paused")
			s.Broadcast(StreamEvent{
				Type: "status",
				Data: map[string]interface{}{"status": "paused"},
			})
		}
		
	case "resume":
		if s.isPlaying && s.isPaused {
			s.isPaused = false
			log.Println("▶️  Playback resumed")
			s.Broadcast(StreamEvent{
				Type: "status",
				Data: map[string]interface{}{"status": "playing"},
			})
		}
		
	case "reset":
		s.currentIndex = 0
		s.isPlaying = false
		s.isPaused = false
		log.Println("🔄 Playback reset")
		s.Broadcast(StreamEvent{
			Type: "status",
			Data: map[string]interface{}{
				"status": "reset",
				"message": "Click Start to begin playback",
			},
		})
	}
}

// Control sends a control command
func (s *StreamServer) Control(cmd string) {
	s.controlChan <- cmd
}

// SetSpeed changes the playback speed
func (s *StreamServer) SetSpeed(speed PlaybackSpeed) {
	s.stateMux.Lock()
	defer s.stateMux.Unlock()
	s.speed = speed
	log.Printf("⚡ Speed set to %.1fx", speed)
	s.Broadcast(StreamEvent{
		Type: "speed",
		Data: map[string]interface{}{"speed": speed},
	})
}

// GetStatus returns current playback status
func (s *StreamServer) GetStatus() map[string]interface{} {
	s.stateMux.RLock()
	defer s.stateMux.RUnlock()
	
	return map[string]interface{}{
		"is_playing":     s.isPlaying,
		"is_paused":      s.isPaused,
		"current_index":  s.currentIndex,
		"total_records":  len(s.records),
		"speed":          s.speed,
	}
}

// SSEHandler handles SSE connections
func (s *StreamServer) SSEHandler(w http.ResponseWriter, r *http.Request) {
	// Set SSE headers
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	
	// Create client channel
	client := make(chan StreamEvent, 100)
	s.AddClient(client)
	defer s.RemoveClient(client)
	
	// Send initial status
	status := s.GetStatus()
	statusData, _ := json.Marshal(status)
	fmt.Fprintf(w, "event: status\ndata: %s\n\n", statusData)
	w.(http.Flusher).Flush()
	
	// Stream events
	for {
		select {
		case event := <-client:
			data, err := json.Marshal(event.Data)
			if err != nil {
				continue
			}
			fmt.Fprintf(w, "event: %s\ndata: %s\n\n", event.Type, data)
			w.(http.Flusher).Flush()
			
		case <-r.Context().Done():
			return
		}
	}
}

// Helper functions for name mapping
func getRuleTypeName(ruleType uint8) string {
	switch ruleType {
	case 0:
		return "Allowlist"
	case 1:
		return "Blocklist"
	default:
		return "Unknown"
	}
}

func getTargetTypeName(targetType uint8) string {
	switch targetType {
	case 0:
		return "Interface-Based"
	case 1:
		return "Prefix-Based"
	default:
		return "Unknown"
	}
}

func getPolicyActionName(action uint8) string {
	switch action {
	case 0:
		return "Permit"
	case 1:
		return "Discard"
	case 2:
		return "Rate-limit"
	case 3:
		return "Redirect"
	default:
		return "Unknown"
	}
}
