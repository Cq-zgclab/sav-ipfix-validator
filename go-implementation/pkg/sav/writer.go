package sav

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"time"
)

// SAVRecordWriter writes SAV IPFIX records to binary format
type SAVRecordWriter struct {
	writer         io.Writer
	exported       uint64
	sequenceNumber uint32
	templatesExported bool
}

// NewSAVRecordWriter creates a new writer for SAV records
func NewSAVRecordWriter(w io.Writer) *SAVRecordWriter {
	return &SAVRecordWriter{
		writer: w,
	}
}

// ExportTemplates writes all SAV template definitions
func (w *SAVRecordWriter) ExportTemplates() error {
	// IPFIX Message Header (16 bytes)
	header := make([]byte, 16)
	binary.BigEndian.PutUint16(header[0:2], 10)                        // Version
	binary.BigEndian.PutUint32(header[4:8], uint32(time.Now().Unix())) // ExportTime
	binary.BigEndian.PutUint32(header[8:12], w.sequenceNumber)         // SequenceNumber
	binary.BigEndian.PutUint32(header[12:16], 0)                       // ObservationDomainId

	// Template Set Header (SetID=2 for template set)
	templateSetID := uint16(2)
	
	// Build all templates
	templates := []struct {
		id     uint16
		fields []templateField
	}{
		{
			id: TemplateIPv4InterfacePrefix,
			fields: []templateField{
				{ieID: 10, length: 4},         // ingressInterface
				{ieID: 8, length: 4},          // sourceIPv4Address
				{ieID: 9, length: 1},          // sourceIPv4PrefixLength
			},
		},
		{
			id: TemplateIPv6InterfacePrefix,
			fields: []templateField{
				{ieID: 10, length: 4},         // ingressInterface
				{ieID: 27, length: 16},        // sourceIPv6Address
				{ieID: 29, length: 1},         // sourceIPv6PrefixLength
			},
		},
		{
			id: TemplateMainDataRecord,
			fields: []templateField{
				{ieID: 323, length: 8},                                // observationTimeMilliseconds
				{ieID: 1, enterpriseID: EnterpriseID, length: 1},     // savRuleType
				{ieID: 2, enterpriseID: EnterpriseID, length: 0xFFFF}, // savTargetType (variable)
				{ieID: 292, length: 0xFFFF},                           // subTemplateList (variable)
				{ieID: 4, enterpriseID: EnterpriseID, length: 1},     // savPolicyAction
			},
		},
	}

	templateSetData := make([]byte, 0, 1024)
	for _, tmpl := range templates {
		// Template Record Header
		templateSetData = append(templateSetData,
			byte(tmpl.id>>8), byte(tmpl.id),          // Template ID
			byte(0), byte(len(tmpl.fields)),           // Field Count
		)
		
		// Fields
		for _, field := range tmpl.fields {
			if field.enterpriseID != 0 {
				// Enterprise field (high bit set)
				templateSetData = append(templateSetData,
					byte(field.ieID>>8)|0x80, byte(field.ieID),
					byte(field.length>>8), byte(field.length),
					byte(field.enterpriseID>>24), byte(field.enterpriseID>>16),
					byte(field.enterpriseID>>8), byte(field.enterpriseID),
				)
			} else {
				// Standard field
				templateSetData = append(templateSetData,
					byte(field.ieID>>8), byte(field.ieID),
					byte(field.length>>8), byte(field.length),
				)
			}
		}
	}

	// Pad to 4-byte boundary
	for len(templateSetData)%4 != 0 {
		templateSetData = append(templateSetData, 0)
	}

	// Template Set Header
	templateSetHeader := make([]byte, 4)
	binary.BigEndian.PutUint16(templateSetHeader[0:2], templateSetID)
	binary.BigEndian.PutUint16(templateSetHeader[2:4], uint16(len(templateSetData)+4))

	// Update message length
	totalLength := 16 + 4 + len(templateSetData)
	binary.BigEndian.PutUint16(header[2:4], uint16(totalLength))

	// Write message
	if _, err := w.writer.Write(header); err != nil {
		return err
	}
	if _, err := w.writer.Write(templateSetHeader); err != nil {
		return err
	}
	if _, err := w.writer.Write(templateSetData); err != nil {
		return err
	}

	w.sequenceNumber++
	w.templatesExported = true
	return nil
}

type templateField struct {
	ieID         uint16
	enterpriseID uint32
	length       uint16
}

// ExportIPv4InterfacePrefix exports an IPv4 interface-to-prefix mapping
func (w *SAVRecordWriter) ExportIPv4InterfacePrefix(
	timestamp time.Time,
	ruleType, targetType, policyAction uint8,
	interfaceID uint32,
	prefix net.IP,
	prefixLen uint8,
) error {
	if !w.templatesExported {
		if err := w.ExportTemplates(); err != nil {
			return fmt.Errorf("failed to export templates: %w", err)
		}
	}

	// Build data record
	data := make([]byte, 0, 256)

	// observationTimeMilliseconds (8 bytes)
	ts := uint64(timestamp.UnixMilli())
	data = append(data,
		byte(ts>>56), byte(ts>>48), byte(ts>>40), byte(ts>>32),
		byte(ts>>24), byte(ts>>16), byte(ts>>8), byte(ts),
	)

	// savRuleType (1 byte)
	data = append(data, ruleType)

	// savTargetType (1 byte)  
	data = append(data, targetType)

	// subTemplateList (variable)
	// SubTemplateList format: semantic (1) + template ID (2) + length (2) + data
	subData := make([]byte, 0, 32)
	
	// Interface ID (4 bytes)
	subData = append(subData,
		byte(interfaceID>>24), byte(interfaceID>>16),
		byte(interfaceID>>8), byte(interfaceID),
	)
	
	// IPv4 prefix (4 bytes)
	ip4 := prefix.To4()
	if ip4 == nil {
		return fmt.Errorf("invalid IPv4 address")
	}
	subData = append(subData, ip4...)
	
	// Prefix length (1 byte)
	subData = append(subData, prefixLen)

	// Write STL header + data with length prefix for variable field
	stlLen := 1 + 2 + 2 + len(subData) // semantic + templateID + length + data
	data = append(data, byte(stlLen))  // Length prefix for variable field
	data = append(data, 0xFF)          // Semantic: allOf
	templateIDBytes := []byte{
		byte(TemplateIPv4InterfacePrefix >> 8),
		byte(TemplateIPv4InterfacePrefix & 0xFF),
	}
	lengthBytes := []byte{
		byte(len(subData) >> 8),
		byte(len(subData) & 0xFF),
	}
	data = append(data, templateIDBytes...)
	data = append(data, lengthBytes...)
	data = append(data, subData...)

	// savPolicyAction (1 byte)
	data = append(data, policyAction)

	return w.writeDataSet(TemplateMainDataRecord, data)
}

// ExportIPv6InterfacePrefix exports an IPv6 interface-to-prefix mapping
func (w *SAVRecordWriter) ExportIPv6InterfacePrefix(
	timestamp time.Time,
	ruleType, targetType, policyAction uint8,
	interfaceID uint32,
	prefix net.IP,
	prefixLen uint8,
) error {
	if !w.templatesExported {
		if err := w.ExportTemplates(); err != nil {
			return fmt.Errorf("failed to export templates: %w", err)
		}
	}

	// Build data record
	data := make([]byte, 0, 256)

	// observationTimeMilliseconds (8 bytes)
	ts := uint64(timestamp.UnixMilli())
	data = append(data,
		byte(ts>>56), byte(ts>>48), byte(ts>>40), byte(ts>>32),
		byte(ts>>24), byte(ts>>16), byte(ts>>8), byte(ts),
	)

	// savRuleType (1 byte)
	data = append(data, ruleType)

	// savTargetType (1 byte)
	data = append(data, targetType)

	// subTemplateList (variable)
	subData := make([]byte, 0, 64)
	
	// Interface ID (4 bytes)
	subData = append(subData,
		byte(interfaceID>>24), byte(interfaceID>>16),
		byte(interfaceID>>8), byte(interfaceID),
	)
	
	// IPv6 prefix (16 bytes)
	ip6 := prefix.To16()
	if ip6 == nil {
		return fmt.Errorf("invalid IPv6 address")
	}
	subData = append(subData, ip6...)
	
	// Prefix length (1 byte)
	subData = append(subData, prefixLen)

	// Write STL header + data with length prefix
	stlLen := 1 + 2 + 2 + len(subData)
	data = append(data, byte(stlLen))
	data = append(data, 0xFF) // Semantic
	templateIDBytes := []byte{
		byte(TemplateIPv6InterfacePrefix >> 8),
		byte(TemplateIPv6InterfacePrefix & 0xFF),
	}
	lengthBytes := []byte{
		byte(len(subData) >> 8),
		byte(len(subData) & 0xFF),
	}
	data = append(data, templateIDBytes...)
	data = append(data, lengthBytes...)
	data = append(data, subData...)

	// savPolicyAction (1 byte)
	data = append(data, policyAction)

	return w.writeDataSet(TemplateMainDataRecord, data)
}

func (w *SAVRecordWriter) writeDataSet(templateID uint16, data []byte) error {
	// Message header (16 bytes)
	header := make([]byte, 16)
	binary.BigEndian.PutUint16(header[0:2], 10) // Version
	binary.BigEndian.PutUint32(header[4:8], uint32(time.Now().Unix()))
	binary.BigEndian.PutUint32(header[8:12], w.sequenceNumber)
	binary.BigEndian.PutUint32(header[12:16], 0)

	// Data Set Header
	dataSetHeader := make([]byte, 4)
	binary.BigEndian.PutUint16(dataSetHeader[0:2], templateID)
	binary.BigEndian.PutUint16(dataSetHeader[2:4], uint16(4+len(data)))

	// Update message length
	totalLength := 16 + 4 + len(data)
	binary.BigEndian.PutUint16(header[2:4], uint16(totalLength))

	// Write
	if _, err := w.writer.Write(header); err != nil {
		return err
	}
	if _, err := w.writer.Write(dataSetHeader); err != nil {
		return err
	}
	if _, err := w.writer.Write(data); err != nil {
		return err
	}

	w.exported++
	w.sequenceNumber++
	return nil
}

// ExportedCount returns the number of records exported
func (w *SAVRecordWriter) ExportedCount() uint64 {
	return w.exported
}

// Close closes the writer
func (w *SAVRecordWriter) Close() error {
	if closer, ok := w.writer.(io.Closer); ok {
		return closer.Close()
	}
	return nil
}
