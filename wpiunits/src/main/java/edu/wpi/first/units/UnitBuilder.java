// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

package edu.wpi.first.units;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.Objects;

/**
 * Builder used for easily deriving new units from existing ones.
 *
 * @param <U> the type of the unit
 */
public final class UnitBuilder<U extends Unit<U>> {
  private final U m_base;
  private UnaryFunction m_fromBase;
  private UnaryFunction m_toBase;
  private String m_name;
  private String m_symbol;

  public UnitBuilder(U base) {
    this.m_base = Objects.requireNonNull(base, "Base unit cannot be null");
  }

  /**
   * Sets the unit conversions based on a simple offset. The new unit will have its values equal to
   * (base value - offset).
   *
   * @param offset the offset
   */
  public UnitBuilder<U> offset(double offset) {
    m_toBase = derivedValue -> derivedValue + offset;
    m_fromBase = baseValue -> baseValue - offset;
    return this;
  }

  /**
   * Maps a value {@code value} in the range {@code [inMin..inMax]} to an output in the range {@code
   * [outMin..outMax]}. Inputs outside the bounds will be mapped correspondingly to outputs outside
   * the output bounds. Inputs equal to {@code inMin} will be mapped to {@code outMin}, and inputs
   * equal to {@code inMax} will similarly be mapped to {@code outMax}.
   *
   * @param value the value to map
   * @param inMin the minimum input value (does not have to be absolute)
   * @param inMax the maximum input value (does not have to be absolute)
   * @param outMin the minimum output value (does not have to be absolute)
   * @param outMax the maximum output value (does not have to be absolute)
   * @return the mapped output
   */
  // NOTE: This method lives here instead of in MappingBuilder because inner classes can't
  // define static methods prior to Java 16.
  private static double mapValue(
      double value, double inMin, double inMax, double outMin, double outMax) {
    return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
  }

  /** Helper class used for safely chaining mapping builder calls. */
  public class MappingBuilder {
    private final double m_minInput;
    private final double m_maxInput;

    private MappingBuilder(double minInput, double maxInput) {
      this.m_minInput = minInput;
      this.m_maxInput = maxInput;
    }

    /**
     * Finalizes the mapping by defining the output range.
     *
     * @param minOutput the minimum output value (does not have to be absolute)
     * @param maxOutput the maximum output value (does not have to be absolute)
     * @return the unit builder, for continued chaining
     */
    public UnitBuilder<U> toOutputRange(double minOutput, double maxOutput) {
      UnitBuilder.this.m_fromBase = x -> mapValue(x, m_minInput, m_maxInput, minOutput, maxOutput);
      UnitBuilder.this.m_toBase = y -> mapValue(y, minOutput, maxOutput, m_minInput, m_maxInput);
      return UnitBuilder.this;
    }
  }

  /**
   * Defines a mapping for values within the given input range. This method call should be
   * immediately followed by {@code .toOutputRange}, eg {@code mappingInputRange(1,
   * 2).toOutputRange(3, 4)}, which will return the unit builder for continued chaining.
   *
   * @param minBase the minimum input value (does not have to be absolute)
   * @param maxBase the maximum output value (does not have to be absolute)
   * @return a builder object used to define the output range
   */
  public MappingBuilder mappingInputRange(double minBase, double maxBase) {
    return new MappingBuilder(minBase, maxBase);
  }

  /**
   * Sets the conversion function to transform values in the base unit to values in the derived
   * unit.
   *
   * @param fromBase the conversion function
   * @return the unit builder, for continued chaining
   */
  public UnitBuilder<U> fromBase(UnaryFunction fromBase) {
    this.m_fromBase = Objects.requireNonNull(fromBase, "fromBase function cannot be null");
    return this;
  }

  /**
   * Sets the conversion function to transform values in the derived unit to values in the base
   * unit.
   *
   * @param toBase the conversion function
   * @return the unit builder, for continued chaining
   */
  public UnitBuilder<U> toBase(UnaryFunction toBase) {
    this.m_toBase = Objects.requireNonNull(toBase, "toBase function cannot be null");
    return this;
  }

  /**
   * Sets the name of the new unit.
   *
   * @param name the new name
   * @return the unit builder, for continued chaining
   */
  public UnitBuilder<U> named(String name) {
    this.m_name = name;
    return this;
  }

  /**
   * Sets the symbol of the new unit.
   *
   * @param symbol the new symbol
   * @return the unit builder, for continued chaining
   */
  public UnitBuilder<U> symbol(String symbol) {
    this.m_symbol = symbol;
    return this;
  }

  /**
   * Helper for defining units that are a scalar fraction of the base unit, such as centimeters
   * being 1/100th of the base unit (meters). The fraction value is specified as the denominator of
   * the fraction, so a centimeter definition would use {@code splitInto(100)} instead of {@code
   * splitInto(1/100.0)}.
   *
   * @param fraction the denominator portion of the fraction of the base unit that a value of 1 in
   *     the derived unit corresponds to
   * @return the unit builder, for continued chaining
   */
  public UnitBuilder<U> splitInto(double fraction) {
    if (fraction == 0) {
      throw new IllegalArgumentException("Fraction must be nonzero");
    }

    return toBase(x -> x / fraction).fromBase(b -> b * fraction);
  }

  /**
   * Helper for defining units that are a scalar multiple of the base unit, such as kilometers being
   * 1000x of the base unit (meters).
   *
   * @param aggregation the magnitude required for a measure in the base unit to equal a magnitude
   *     of 1 in the derived unit
   * @return the unit builder, for continued chaining
   */
  public UnitBuilder<U> aggregate(double aggregation) {
    if (aggregation == 0) {
      throw new IllegalArgumentException("Aggregation amount must be nonzero");
    }

    return toBase(x -> x * aggregation).fromBase(b -> b / aggregation);
  }

  /**
   * Creates the new unit based off of the builder methods called prior.
   *
   * @return the new derived unit
   * @throws NullPointerException if the unit conversions, unit name, or unit symbol were not set
   * @throws RuntimeException if the base unit does not define a constructor accepting the
   *     conversion functions, unit name, and unit symbol - in that order
   */
  @SuppressWarnings("PMD.AvoidAccessibilityAlteration")
  public U make() {
    Objects.requireNonNull(m_fromBase, "fromBase function was not set");
    Objects.requireNonNull(m_toBase, "toBase function was not set");
    Objects.requireNonNull(m_name, "new unit name was not set");
    Objects.requireNonNull(m_symbol, "new unit symbol was not set");
    Class<? extends U> baseType = m_base.m_baseType;
    try {
      Constructor<? extends U> ctor =
          baseType.getDeclaredConstructor(
              UnaryFunction.class, // toBaseUnits
              UnaryFunction.class, // fromBaseUnits
              String.class, // name
              String.class); // symbol
      // need to flag the constructor as accessible so we can use private, package-private, and
      // protected constructors
      ctor.setAccessible(true);
      return ctor.newInstance(
          m_toBase.pipeTo(m_base.getConverterToBase()),
          m_base.getConverterFromBase().pipeTo(m_fromBase),
          m_name,
          m_symbol);
    } catch (InstantiationException e) {
      throw new RuntimeException("Could not instantiate class " + baseType.getName(), e);
    } catch (IllegalAccessException e) {
      throw new RuntimeException("Could not access constructor", e);
    } catch (InvocationTargetException e) {
      throw new RuntimeException("Constructing " + baseType.getName() + " raised an exception", e);
    } catch (NoSuchMethodException e) {
      throw new RuntimeException("No compatible constructor", e);
    }
  }
}
