#ifndef _MCP3221_H_
#define _MCP3221_H_

void mcp3221_init(void);
bool mcp3221_read(uint8 addr, uint16 *pData);

#endif