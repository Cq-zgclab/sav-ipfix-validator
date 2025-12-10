package sav

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"time"
)

// SAVRecordReader reads SAV IPFIX records from binary format
type SAVRecordReader struct {
	reader    io.Reader
	collected uint64
}

// NewSAVRecordReader creates a new reader for SAV records
func NewSAVRecordReader(r io.Reader) *SAVRecordReader {
	return &SAVRecordReader{
		reader: r,
	}
}

// CollectedRecord represents a collected SAV record
type CollectedRecord struct {
	Timestamp    time.Time
	RuleType     uint8
	TargetType   uint8
	PolicyAction uint8
	Mappings     []Mapping
}

// Mapping represents a SAV mapping
type Mapping struct {
	InterfaceID uint32
	Prefix      net.IP
	PrefixLen   uint8
	IsIPv6      bool
}

// ReadRecord reads the next SAV record
func (r *SAVRecordReader) ReadRecord() (*CollectedRecord, error) {
	// Read message header (16 bytes)
	header := make([]byte, 16)
	if _, err := io.ReadFull(r.reader, header); err != nil {
		return nil, err
	}

	version := binary.BigEndian.Uint16(header[0:2])
	if version != 10 {
		return nil, fmt.Errorf("unsupported IPFIX version: %d", version)
	}

	length := binary.BigEndian.Uint16(header[2:4])
	// Read the rest of the message
	payload := make([]byte, length-16)
	if _, err := io.ReadFull(r.reader, payload); err != nil {
		return nil, err
	}

	// Parse sets
	offset := 0
	for offset < len(payload) {
		if offset+4 > len(payload) {
			break
		}

		setID := binary.BigEndian.Uint16(payload[offset : offset+2])
		setLen := binary.BigEndian.Uint16(payload[offset+2 : offset+4])

		if setLen < 4 {
			return nil, fmt.Errorf("invalid set length: %d", setLen)
		}

		setData := payload[offset+4 : offset+int(setLen)]

		// Skip template sets (setID < 256)
		if setID >= 256 {
			// This is a data set
			if setID == TemplateMainDataRecord {
				record, err := r.parseDataRecord(setData)
				if err != nil {
					return nil, fmt.Errorf("failed to parse data record: %w", err)
				}
				r.collected++
				return record, nil
			}
		}

		offset += int(setLen)
	}

	// No data record found, read next message
	return r.ReadRecord()
}

func (r *SAVRecordReader) parseDataRecord(data []byte) (*CollectedRecord, error) {
	record := &CollectedRecord{}
	offset := 0

	// observationTimeMilliseconds (8 bytes)
	if offset+8 > len(data) {
		return nil, fmt.Errorf("insufficient data for timestamp")
	}
	ts := binary.BigEndian.Uint64(data[offset : offset+8])
	record.Timestamp = time.UnixMilli(int64(ts))
	offset += 8

	// savRuleType (1 byte)
	if offset+1 > len(data) {
		return nil, fmt.Errorf("insufficient data for ruleType")
	}
	record.RuleType = data[offset]
	offset++

	// savTargetType (1 byte)
	if offset+1 > len(data) {
		return nil, fmt.Errorf("insufficient data for targetType")
	}
	record.TargetType = data[offset]
	offset++

	// subTemplateList (variable with length prefix)
	if offset+1 > len(data) {
		return nil, fmt.Errorf("insufficient data for STL length")
	}
	stlLen := int(data[offset])
	offset++

	if offset+stlLen > len(data) {
		return nil, fmt.Errorf("insufficient data for STL")
	}
	stlData := data[offset : offset+stlLen]
	offset += stlLen

	// Parse STL
	mappings, err := r.parseSubTemplateList(stlData)
	if err != nil {
		return nil, fmt.Errorf("failed to parse STL: %w", err)
	}
	record.Mappings = mappings

	// savPolicyAction (1 byte)
	if offset+1 > len(data) {
		return nil, fmt.Errorf("insufficient data for policyAction")
	}
	record.PolicyAction = data[offset]

	return record, nil
}

func (r *SAVRecordReader) parseSubTemplateList(data []byte) ([]Mapping, error) {
	offset := 0
	var mappings []Mapping

	// Semantic (1 byte)
	if offset+1 > len(data) {
		return nil, fmt.Errorf("insufficient STL data for semantic")
	}
	offset++ // skip semantic

	// Template ID (2 bytes)
	if offset+2 > len(data) {
		return nil, fmt.Errorf("insufficient STL data for template ID")
	}
	templateID := binary.BigEndian.Uint16(data[offset : offset+2])
	offset += 2

	// Length (2 bytes)
	if offset+2 > len(data) {
		return nil, fmt.Errorf("insufficient STL data for length")
	}
	length := binary.BigEndian.Uint16(data[offset : offset+2])
	offset += 2

	// Sub-records
	if offset+int(length) > len(data) {
		return nil, fmt.Errorf("insufficient STL data for records")
	}
	recordData := data[offset : offset+int(length)]

	switch templateID {
	case TemplateIPv4InterfacePrefix:
		// Parse IPv4 records
		recOffset := 0
		for recOffset < len(recordData) {
			if recOffset+9 > len(recordData) {
				break
			}

			mapping := Mapping{
				InterfaceID: binary.BigEndian.Uint32(recordData[recOffset : recOffset+4]),
				Prefix:      net.IP(recordData[recOffset+4 : recOffset+8]),
				PrefixLen:   recordData[recOffset+8],
				IsIPv6:      false,
			}
			mappings = append(mappings, mapping)
			recOffset += 9
		}

	case TemplateIPv6InterfacePrefix:
		// Parse IPv6 records
		recOffset := 0
		for recOffset < len(recordData) {
			if recOffset+21 > len(recordData) {
				break
			}

			mapping := Mapping{
				InterfaceID: binary.BigEndian.Uint32(recordData[recOffset : recOffset+4]),
				Prefix:      net.IP(recordData[recOffset+4 : recOffset+20]),
				PrefixLen:   recordData[recOffset+20],
				IsIPv6:      true,
			}
			mappings = append(mappings, mapping)
			recOffset += 21
		}

	default:
		return nil, fmt.Errorf("unknown template ID: %d", templateID)
	}

	return mappings, nil
}

// CollectedCount returns the number of records collected
func (r *SAVRecordReader) CollectedCount() uint64 {
	return r.collected
}
