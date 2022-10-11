#pragma once

#include <ble/Machine.h>
#include <ble/Scale.h>

namespace ble {

class Devices {
 public:
  ble::Scale* getScale() { return m_scale; }
  ble::Machine* getMachine() { return m_machine; }

  void setScale(ble::Scale* s) { m_scale = s; }
  void setMachine(ble::Machine* m) { m_machine = m; }

 private:
  ble::Machine* m_machine = nullptr;
  ble::Scale* m_scale = nullptr;
};
}  // namespace ble