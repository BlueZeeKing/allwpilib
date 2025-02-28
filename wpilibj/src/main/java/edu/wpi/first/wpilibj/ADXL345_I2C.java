// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

package edu.wpi.first.wpilibj;

import edu.wpi.first.hal.FRCNetComm.tInstances;
import edu.wpi.first.hal.FRCNetComm.tResourceType;
import edu.wpi.first.hal.HAL;
import edu.wpi.first.hal.SimDevice;
import edu.wpi.first.hal.SimDouble;
import edu.wpi.first.hal.SimEnum;
import edu.wpi.first.networktables.DoublePublisher;
import edu.wpi.first.networktables.DoubleTopic;
import edu.wpi.first.networktables.NTSendable;
import edu.wpi.first.networktables.NTSendableBuilder;
import edu.wpi.first.util.sendable.SendableRegistry;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * ADXL345 I2C Accelerometer.
 *
 * <p>The Onboard I2C port is subject to system lockups. See <a
 * href="https://docs.wpilib.org/en/stable/docs/yearly-overview/known-issues.html#onboard-i2c-causing-system-lockups">
 * WPILib Known Issues</a> page for details.
 */
@SuppressWarnings({"TypeName", "PMD.UnusedPrivateField"})
public class ADXL345_I2C implements NTSendable, AutoCloseable {
  private static final byte kAddress = 0x1D;
  private static final byte kPowerCtlRegister = 0x2D;
  private static final byte kDataFormatRegister = 0x31;
  private static final byte kDataRegister = 0x32;
  private static final double kGsPerLSB = 0.00390625;
  private static final byte kPowerCtl_Link = 0x20;
  private static final byte kPowerCtl_AutoSleep = 0x10;
  private static final byte kPowerCtl_Measure = 0x08;
  private static final byte kPowerCtl_Sleep = 0x04;

  private static final byte kDataFormat_SelfTest = (byte) 0x80;
  private static final byte kDataFormat_SPI = 0x40;
  private static final byte kDataFormat_IntInvert = 0x20;
  private static final byte kDataFormat_FullRes = 0x08;
  private static final byte kDataFormat_Justify = 0x04;

  public enum Range {
    k2G,
    k4G,
    k8G,
    k16G
  }

  public enum Axes {
    kX((byte) 0x00),
    kY((byte) 0x02),
    kZ((byte) 0x04);

    /** The integer value representing this enumeration. */
    public final byte value;

    Axes(byte value) {
      this.value = value;
    }
  }

  @SuppressWarnings("MemberName")
  public static class AllAxes {
    public double XAxis;
    public double YAxis;
    public double ZAxis;
  }

  protected I2C m_i2c;

  protected SimDevice m_simDevice;
  protected SimEnum m_simRange;
  protected SimDouble m_simX;
  protected SimDouble m_simY;
  protected SimDouble m_simZ;

  /**
   * Constructs the ADXL345 Accelerometer with I2C address 0x1D.
   *
   * @param port The I2C port the accelerometer is attached to
   * @param range The range (+ or -) that the accelerometer will measure.
   */
  public ADXL345_I2C(I2C.Port port, Range range) {
    this(port, range, kAddress);
  }

  /**
   * Constructs the ADXL345 Accelerometer over I2C.
   *
   * @param port The I2C port the accelerometer is attached to
   * @param range The range (+ or -) that the accelerometer will measure.
   * @param deviceAddress I2C address of the accelerometer (0x1D or 0x53)
   */
  public ADXL345_I2C(I2C.Port port, Range range, int deviceAddress) {
    m_i2c = new I2C(port, deviceAddress);

    // simulation
    m_simDevice = SimDevice.create("Accel:ADXL345_I2C", port.value, deviceAddress);
    if (m_simDevice != null) {
      m_simRange =
          m_simDevice.createEnumDouble(
              "range",
              SimDevice.Direction.kOutput,
              new String[] {"2G", "4G", "8G", "16G"},
              new double[] {2.0, 4.0, 8.0, 16.0},
              0);
      m_simX = m_simDevice.createDouble("x", SimDevice.Direction.kInput, 0.0);
      m_simY = m_simDevice.createDouble("y", SimDevice.Direction.kInput, 0.0);
      m_simZ = m_simDevice.createDouble("z", SimDevice.Direction.kInput, 0.0);
    }

    // Turn on the measurements
    m_i2c.write(kPowerCtlRegister, kPowerCtl_Measure);

    setRange(range);

    HAL.report(tResourceType.kResourceType_ADXL345, tInstances.kADXL345_I2C);
    SendableRegistry.addLW(this, "ADXL345_I2C", port.value);
  }

  public int getPort() {
    return m_i2c.getPort();
  }

  public int getDeviceAddress() {
    return m_i2c.getDeviceAddress();
  }

  @Override
  public void close() {
    SendableRegistry.remove(this);
    if (m_i2c != null) {
      m_i2c.close();
      m_i2c = null;
    }
    if (m_simDevice != null) {
      m_simDevice.close();
      m_simDevice = null;
    }
  }

  /**
   * Set the measuring range of the accelerometer.
   *
   * @param range The maximum acceleration, positive or negative, that the accelerometer will
   *     measure.
   */
  public void setRange(Range range) {
    final byte value;
    switch (range) {
      case k2G:
        value = 0;
        break;
      case k4G:
        value = 1;
        break;
      case k8G:
        value = 2;
        break;
      case k16G:
        value = 3;
        break;
      default:
        throw new IllegalArgumentException("Missing case for range type " + range);
    }

    // Specify the data format to read
    m_i2c.write(kDataFormatRegister, kDataFormat_FullRes | value);

    if (m_simRange != null) {
      m_simRange.set(value);
    }
  }

  /**
   * Returns the acceleration along the X axis in g-forces.
   *
   * @return The acceleration along the X axis in g-forces.
   */
  public double getX() {
    return getAcceleration(Axes.kX);
  }

  /**
   * Returns the acceleration along the Y axis in g-forces.
   *
   * @return The acceleration along the Y axis in g-forces.
   */
  public double getY() {
    return getAcceleration(Axes.kY);
  }

  /**
   * Returns the acceleration along the Z axis in g-forces.
   *
   * @return The acceleration along the Z axis in g-forces.
   */
  public double getZ() {
    return getAcceleration(Axes.kZ);
  }

  /**
   * Get the acceleration of one axis in Gs.
   *
   * @param axis The axis to read from.
   * @return Acceleration of the ADXL345 in Gs.
   */
  public double getAcceleration(Axes axis) {
    if (axis == Axes.kX && m_simX != null) {
      return m_simX.get();
    }
    if (axis == Axes.kY && m_simY != null) {
      return m_simY.get();
    }
    if (axis == Axes.kZ && m_simZ != null) {
      return m_simZ.get();
    }
    ByteBuffer rawAccel = ByteBuffer.allocate(2);
    m_i2c.read(kDataRegister + axis.value, 2, rawAccel);

    // Sensor is little endian... swap bytes
    rawAccel.order(ByteOrder.LITTLE_ENDIAN);
    return rawAccel.getShort(0) * kGsPerLSB;
  }

  /**
   * Get the acceleration of all axes in Gs.
   *
   * @return An object containing the acceleration measured on each axis of the ADXL345 in Gs.
   */
  public AllAxes getAccelerations() {
    AllAxes data = new AllAxes();
    if (m_simX != null && m_simY != null && m_simZ != null) {
      data.XAxis = m_simX.get();
      data.YAxis = m_simY.get();
      data.ZAxis = m_simZ.get();
      return data;
    }
    ByteBuffer rawData = ByteBuffer.allocate(6);
    m_i2c.read(kDataRegister, 6, rawData);

    // Sensor is little endian... swap bytes
    rawData.order(ByteOrder.LITTLE_ENDIAN);
    data.XAxis = rawData.getShort(0) * kGsPerLSB;
    data.YAxis = rawData.getShort(2) * kGsPerLSB;
    data.ZAxis = rawData.getShort(4) * kGsPerLSB;
    return data;
  }

  @Override
  public void initSendable(NTSendableBuilder builder) {
    builder.setSmartDashboardType("3AxisAccelerometer");
    DoublePublisher pubX = new DoubleTopic(builder.getTopic("X")).publish();
    DoublePublisher pubY = new DoubleTopic(builder.getTopic("Y")).publish();
    DoublePublisher pubZ = new DoubleTopic(builder.getTopic("Z")).publish();
    builder.addCloseable(pubX);
    builder.addCloseable(pubY);
    builder.addCloseable(pubZ);
    builder.setUpdateTable(
        () -> {
          AllAxes data = getAccelerations();
          pubX.set(data.XAxis);
          pubY.set(data.YAxis);
          pubZ.set(data.ZAxis);
        });
  }
}
