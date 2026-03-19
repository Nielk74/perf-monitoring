-- ============================================================================
-- STORED PROCEDURES - TRADE VALIDATION
-- Created: 2004-03-20 (V1)
-- Updated: 2016-09-01 (V4)
-- Author: Multiple contributors (see revision history)
-- Database: MS SQL Server 2000/2005/2008/2014/2016
-- ============================================================================
-- $Header: /cvsroot/trading/db/sp_trade_validation.sql,v 1.89 2019/06/15 14:20:00 apatel Exp $
-- $Revision: 1.89 $
-- ============================================================================
--
-- PROCEDURES IN THIS FILE:
-- 1. SP_VALIDATE_TRADE - Main trade validation
-- 2. SP_VALIDATE_COUNTERPARTY - Counterparty validation
-- 3. SP_CALCULATE_EXPOSURE - Calculate current exposure
-- 4. SP_GET_TRADE_BY_REF - Get trade by reference
-- 5. SP_INSERT_TRADE - Insert new trade (V1)
-- 6. SP_INSERT_TRADE_V2 - Insert trade with notional (V3+)
-- 7. SP_UPDATE_TRADE_STATUS - Update trade status
-- 8. SP_GET_DAILY_TRADES - Get daily trade report
--
-- ============================================================================

USE [trading_db]
GO

-- ============================================================================
-- SP_VALIDATE_TRADE
-- ============================================================================
-- Validates a trade before booking
-- Returns: 0 = Valid, >0 = Error code
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_VALIDATE_TRADE' AND type = 'P')
    DROP PROCEDURE SP_VALIDATE_TRADE
GO

CREATE PROCEDURE SP_VALIDATE_TRADE
    @p_trd_ref_nm       VARCHAR(24),
    @p_cpty_id          INT,
    @p_prd_typ_id       SMALLINT,
    @p_trd_dt           DATETIME,
    @p_eff_dt           DATETIME,
    @p_mat_dt           DATETIME = NULL,
    @p_ntnl_amt         DECIMAL(22,2) = NULL,
    @p_ntnl_ccy_cd      CHAR(3) = 'USD',
    @p_err_code         INT OUTPUT,
    @p_err_msg          VARCHAR(256) OUTPUT
AS
BEGIN
    SET NOCOUNT ON
    
    DECLARE @v_cpty_sttus CHAR(1)
    DECLARE @v_prd_sttus CHAR(1)
    DECLARE @v_cpty_lmt_amt DECIMAL(18,2)
    DECLARE @v_current_expo DECIMAL(18,2)
    
    -- Initialize outputs
    SELECT @p_err_code = 0, @p_err_msg = ''
    
    -- Check for duplicate trade reference
    IF EXISTS (SELECT 1 FROM TRD_BLOTTER WHERE TRD_REF_NM = @p_trd_ref_nm)
    BEGIN
        SELECT @p_err_code = 1001, @p_err_msg = 'Duplicate trade reference: ' + @p_trd_ref_nm
        RETURN @p_err_code
    END
    
    -- Validate counterparty exists and is active
    SELECT @v_cpty_sttus = CPTY_STTUS_CD 
    FROM CPTY_MSTR 
    WHERE CPTY_ID = @p_cpty_id
    
    IF @@ROWCOUNT = 0
    BEGIN
        SELECT @p_err_code = 1002, @p_err_msg = 'Counterparty not found: ' + CAST(@p_cpty_id AS VARCHAR)
        RETURN @p_err_code
    END
    
    IF @v_cpty_sttus <> 'A'
    BEGIN
        SELECT @p_err_code = 1003, @p_err_msg = 'Counterparty is not active'
        RETURN @p_err_code
    END
    
    -- Validate product type exists and is active
    SELECT @v_prd_sttus = PRD_TYP_STTUS 
    FROM PRD_TYP_REF 
    WHERE PRD_TYP_ID = @p_prd_typ_id
    
    IF @@ROWCOUNT = 0
    BEGIN
        SELECT @p_err_code = 1004, @p_err_msg = 'Product type not found: ' + CAST(@p_prd_typ_id AS VARCHAR)
        RETURN @p_err_code
    END
    
    IF @v_prd_sttus <> 'A'
    BEGIN
        SELECT @p_err_code = 1005, @p_err_msg = 'Product type is not active'
        RETURN @p_err_code
    END
    
    -- Validate dates
    IF @p_eff_dt < @p_trd_dt
    BEGIN
        SELECT @p_err_code = 1006, @p_err_msg = 'Effective date cannot be before trade date'
        RETURN @p_err_code
    END
    
    IF @p_mat_dt IS NOT NULL AND @p_mat_dt <= @p_eff_dt
    BEGIN
        SELECT @p_err_code = 1007, @p_err_msg = 'Maturity date must be after effective date'
        RETURN @p_err_code
    END
    
    -- Validate notional amount (V3+)
    -- #ifdef SCHEMA_V3_NOTIONAL
    IF @p_ntnl_amt IS NOT NULL AND @p_ntnl_amt <= 0
    BEGIN
        SELECT @p_err_code = 1008, @p_err_msg = 'Notional amount must be positive'
        RETURN @p_err_code
    END
    -- #endif
    
    -- Check counterparty limit (V2+)
    -- #ifdef SCHEMA_V2_COUNTERPARTY
    SELECT @v_cpty_lmt_amt = LMT_AMT 
    FROM CPTY_LMT 
    WHERE CPTY_ID = @p_cpty_id 
      AND LMT_TYP_CD = 'CRD_EXPO'
      AND (EXP_DT IS NULL OR EXP_DT > GETDATE())
    
    IF @v_cpty_lmt_amt IS NOT NULL AND @p_ntnl_amt IS NOT NULL
    BEGIN
        -- Calculate current exposure
        SELECT @v_current_expo = ISNULL(SUM(ABS(NTNL_AMT)), 0)
        FROM TRD_BLOTTER
        WHERE CPTY_ID = @p_cpty_id
          AND TRD_STTUS_CD IN ('PE', 'VF', 'BK')
        
        IF (@v_current_expo + @p_ntnl_amt) > @v_cpty_lmt_amt
        BEGIN
            SELECT @p_err_code = 1009, @p_err_msg = 'Counterparty limit exceeded'
            RETURN @p_err_code
        END
    END
    -- #endif
    
    RETURN 0
END
GO

-- ============================================================================
-- SP_VALIDATE_COUNTERPARTY
-- ============================================================================
-- Validates counterparty for trading
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_VALIDATE_COUNTERPARTY' AND type = 'P')
    DROP PROCEDURE SP_VALIDATE_COUNTERPARTY
GO

CREATE PROCEDURE SP_VALIDATE_COUNTERPARTY
    @p_cpty_id          INT,
    @p_trd_amt          DECIMAL(22,2) = NULL,
    @p_is_valid         BIT OUTPUT,
    @p_err_msg          VARCHAR(256) OUTPUT
AS
BEGIN
    SET NOCOUNT ON
    
    DECLARE @v_cpty_sttus CHAR(1)
    DECLARE @v_cpty_cd VARCHAR(12)
    DECLARE @v_cpty_nm VARCHAR(64)
    DECLARE @v_lmt_amt DECIMAL(18,2)
    DECLARE @v_current_expo DECIMAL(18,2)
    
    SELECT @p_is_valid = 0, @p_err_msg = ''
    
    -- Get counterparty details
    SELECT 
        @v_cpty_sttus = CPTY_STTUS_CD,
        @v_cpty_cd = CPTY_CD,
        @v_cpty_nm = CPTY_NM
    FROM CPTY_MSTR
    WHERE CPTY_ID = @p_cpty_id
    
    IF @@ROWCOUNT = 0
    BEGIN
        SELECT @p_err_msg = 'Counterparty ID ' + CAST(@p_cpty_id AS VARCHAR) + ' not found'
        RETURN
    END
    
    IF @v_cpty_sttus <> 'A'
    BEGIN
        SELECT @p_err_msg = 'Counterparty ' + @v_cpty_cd + ' is not active (status: ' + @v_cpty_sttus + ')'
        RETURN
    END
    
    -- Check limit if amount provided
    IF @p_trd_amt IS NOT NULL
    BEGIN
        SELECT @v_lmt_amt = LMT_AMT
        FROM CPTY_LMT
        WHERE CPTY_ID = @p_cpty_id
          AND LMT_TYP_CD = 'CRD_EXPO'
          AND (EXP_DT IS NULL OR EXP_DT > GETDATE())
        
        IF @v_lmt_amt IS NOT NULL
        BEGIN
            SELECT @v_current_expo = ISNULL(SUM(ABS(NTNL_AMT)), 0)
            FROM TRD_BLOTTER
            WHERE CPTY_ID = @p_cpty_id
              AND TRD_STTUS_CD IN ('PE', 'VF', 'BK')
            
            IF (@v_current_expo + @p_trd_amt) > @v_lmt_amt
            BEGIN
                SELECT @p_err_msg = 'Limit exceeded for ' + @v_cpty_nm + 
                       ' (Current: ' + CAST(@v_current_expo AS VARCHAR) + 
                       ', Limit: ' + CAST(@v_lmt_amt AS VARCHAR) + 
                       ', Requested: ' + CAST(@p_trd_amt AS VARCHAR) + ')'
                RETURN
            END
        END
    END
    
    SELECT @p_is_valid = 1
END
GO

-- ============================================================================
-- SP_CALCULATE_EXPOSURE
-- ============================================================================
-- Calculates current exposure for a counterparty
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_CALCULATE_EXPOSURE' AND type = 'P')
    DROP PROCEDURE SP_CALCULATE_EXPOSURE
GO

CREATE PROCEDURE SP_CALCULATE_EXPOSURE
    @p_cpty_id          INT,
    @p_expo_amt         DECIMAL(22,2) OUTPUT,
    @p_trd_cnt          INT OUTPUT
AS
BEGIN
    SET NOCOUNT ON
    
    SELECT 
        @p_expo_amt = ISNULL(SUM(ABS(NTNL_AMT)), 0),
        @p_trd_cnt = COUNT(*)
    FROM TRD_BLOTTER
    WHERE CPTY_ID = @p_cpty_id
      AND TRD_STTUS_CD IN ('PE', 'VF', 'BK')
      AND MAT_DT > GETDATE()
END
GO

-- ============================================================================
-- SP_GET_TRADE_BY_REF
-- ============================================================================
-- Retrieves trade by reference number
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_GET_TRADE_BY_REF' AND type = 'P')
    DROP PROCEDURE SP_GET_TRADE_BY_REF
GO

CREATE PROCEDURE SP_GET_TRADE_BY_REF
    @p_trd_ref_nm       VARCHAR(24)
AS
BEGIN
    SET NOCOUNT ON
    
    SELECT 
        t.TRD_ID,
        t.TRD_REF_NM,
        t.CPTY_ID,
        c.CPTY_CD,
        c.CPTY_NM,
        t.PRD_TYP_ID,
        p.PRD_TYP_CD,
        p.PRD_TYP_NM,
        t.TRD_STTUS_CD,
        t.TRD_DT,
        t.EFF_DT,
        t.MAT_DT,
        t.CCY_CD,
        t.BOOK_NM,
        t.TRD_USR_ID,
        -- V3 columns
        t.NTNL_AMT,
        t.NTNL_CCY_CD,
        t.NTNL_AMT_USD,
        t.MTM_VAL,
        t.MTM_CCY_CD,
        -- V4 columns
        t.TRD_VERS_NUM,
        t.USI_CD,
        t.UTI_CD,
        t.CLR_STS_CD,
        t.CRTD_TS,
        t.UPDTD_TS
    FROM TRD_BLOTTER t
    INNER JOIN CPTY_MSTR c ON t.CPTY_ID = c.CPTY_ID
    INNER JOIN PRD_TYP_REF p ON t.PRD_TYP_ID = p.PRD_TYP_ID
    WHERE t.TRD_REF_NM = @p_trd_ref_nm
END
GO

-- ============================================================================
-- SP_INSERT_TRADE (V1 - Legacy)
-- ============================================================================
-- Original insert procedure from 2004
-- DEPRECATED: Use SP_INSERT_TRADE_V2 for new code
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_INSERT_TRADE' AND type = 'P')
    DROP PROCEDURE SP_INSERT_TRADE
GO

CREATE PROCEDURE SP_INSERT_TRADE
    @p_trd_ref_nm       VARCHAR(24),
    @p_cpty_id          INT,
    @p_prd_typ_id       SMALLINT,
    @p_trd_dt           DATETIME,
    @p_eff_dt           DATETIME,
    @p_mat_dt           DATETIME = NULL,
    @p_ccy_cd           CHAR(3) = 'USD',
    @p_book_nm          VARCHAR(16),
    @p_trd_usr_id       VARCHAR(16),
    @p_trd_id           INT OUTPUT,
    @p_err_code         INT OUTPUT,
    @p_err_msg          VARCHAR(256) OUTPUT
AS
BEGIN
    SET NOCOUNT ON
    
    DECLARE @v_err_code INT
    DECLARE @v_err_msg VARCHAR(256)
    
    -- Validate trade
    EXEC SP_VALIDATE_TRADE 
        @p_trd_ref_nm,
        @p_cpty_id,
        @p_prd_typ_id,
        @p_trd_dt,
        @p_eff_dt,
        @p_mat_dt,
        NULL,  -- @p_ntnl_amt (not supported in V1)
        'USD',
        @v_err_code OUTPUT,
        @v_err_msg OUTPUT
    
    IF @v_err_code <> 0
    BEGIN
        SELECT @p_err_code = @v_err_code, @p_err_msg = @v_err_msg
        RETURN @p_err_code
    END
    
    -- Insert trade
    INSERT INTO TRD_BLOTTER (
        TRD_REF_NM,
        CPTY_ID,
        PRD_TYP_ID,
        TRD_STTUS_CD,
        TRD_DT,
        EFF_DT,
        MAT_DT,
        CCY_CD,
        BOOK_NM,
        TRD_USR_ID
    ) VALUES (
        @p_trd_ref_nm,
        @p_cpty_id,
        @p_prd_typ_id,
        'PE',
        @p_trd_dt,
        @p_eff_dt,
        @p_mat_dt,
        @p_ccy_cd,
        @p_book_nm,
        @p_trd_usr_id
    )
    
    SELECT @p_trd_id = SCOPE_IDENTITY(), @p_err_code = 0, @p_err_msg = ''
    
    RETURN 0
END
GO

-- ============================================================================
-- SP_INSERT_TRADE_V2 (V3+)
-- ============================================================================
-- Insert trade with notional amount
-- Supports V3+ schema features
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_INSERT_TRADE_V2' AND type = 'P')
    DROP PROCEDURE SP_INSERT_TRADE_V2
GO

CREATE PROCEDURE SP_INSERT_TRADE_V2
    @p_trd_ref_nm       VARCHAR(24),
    @p_cpty_id          INT,
    @p_prd_typ_id       SMALLINT,
    @p_trd_dt           DATETIME,
    @p_eff_dt           DATETIME,
    @p_mat_dt           DATETIME = NULL,
    @p_ntnl_amt         DECIMAL(22,2) = NULL,
    @p_ntnl_ccy_cd      CHAR(3) = 'USD',
    @p_ntnl_amt_usd     DECIMAL(22,2) = NULL,
    @p_pay_fixed_flg    CHAR(1) = NULL,
    @p_fixed_rate       DECIMAL(12,8) = NULL,
    @p_flt_idx_cd       VARCHAR(8) = NULL,
    @p_flt_spread       DECIMAL(12,8) = NULL,
    @p_book_nm          VARCHAR(16),
    @p_trd_usr_id       VARCHAR(16),
    @p_trd_id           INT OUTPUT,
    @p_err_code         INT OUTPUT,
    @p_err_msg          VARCHAR(256) OUTPUT
AS
BEGIN
    SET NOCOUNT ON
    
    DECLARE @v_err_code INT
    DECLARE @v_err_msg VARCHAR(256)
    DECLARE @v_trd_id INT
    
    -- Validate trade
    EXEC SP_VALIDATE_TRADE 
        @p_trd_ref_nm,
        @p_cpty_id,
        @p_prd_typ_id,
        @p_trd_dt,
        @p_eff_dt,
        @p_mat_dt,
        @p_ntnl_amt,
        @p_ntnl_ccy_cd,
        @v_err_code OUTPUT,
        @v_err_msg OUTPUT
    
    IF @v_err_code <> 0
    BEGIN
        SELECT @p_err_code = @v_err_code, @p_err_msg = @v_err_msg
        RETURN @p_err_code
    END
    
    -- Begin transaction for atomic insert
    BEGIN TRANSACTION
    
    BEGIN TRY
        -- Insert trade
        INSERT INTO TRD_BLOTTER (
            TRD_REF_NM,
            CPTY_ID,
            PRD_TYP_ID,
            TRD_STTUS_CD,
            TRD_DT,
            EFF_DT,
            MAT_DT,
            CCY_CD,
            NTNL_AMT,
            NTNL_CCY_CD,
            NTNL_AMT_USD,
            PAY_FIXED_FLG,
            FIXED_RATE,
            FLT_IDX_CD,
            FLT_SPREAD,
            BOOK_NM,
            TRD_USR_ID
        ) VALUES (
            @p_trd_ref_nm,
            @p_cpty_id,
            @p_prd_typ_id,
            'PE',
            @p_trd_dt,
            @p_eff_dt,
            @p_mat_dt,
            @p_ntnl_ccy_cd,
            @p_ntnl_amt,
            @p_ntnl_ccy_cd,
            @p_ntnl_amt_usd,
            @p_pay_fixed_flg,
            @p_fixed_rate,
            @p_flt_idx_cd,
            @p_flt_spread,
            @p_book_nm,
            @p_trd_usr_id
        )
        
        SELECT @v_trd_id = SCOPE_IDENTITY()
        
        -- Insert history record (V4)
        -- #ifdef SCHEMA_V4_CURRENT
        INSERT INTO TRD_HIST (
            TRD_ID, TRD_VERS_NUM, TRD_STTUS_CD, NTNL_AMT, MTM_VAL,
            CHNG_TYPE_CD, CHNG_BY_USR
        ) VALUES (
            @v_trd_id, 1, 'PE', @p_ntnl_amt, NULL,
            'NEW', @p_trd_usr_id
        )
        -- #endif
        
        COMMIT TRANSACTION
        
        SELECT @p_trd_id = @v_trd_id, @p_err_code = 0, @p_err_msg = ''
        
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION
        
        SELECT @p_err_code = ERROR_NUMBER(), 
               @p_err_msg = ERROR_MESSAGE()
        
        RETURN @p_err_code
    END CATCH
    
    RETURN 0
END
GO

-- ============================================================================
-- SP_UPDATE_TRADE_STATUS
-- ============================================================================
-- Updates trade status with history tracking
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_UPDATE_TRADE_STATUS' AND type = 'P')
    DROP PROCEDURE SP_UPDATE_TRADE_STATUS
GO

CREATE PROCEDURE SP_UPDATE_TRADE_STATUS
    @p_trd_id           INT,
    @p_new_sttus_cd     CHAR(2),
    @p_chng_reason_tx   VARCHAR(256) = NULL,
    @p_chng_by_usr      VARCHAR(32),
    @p_err_code         INT OUTPUT,
    @p_err_msg          VARCHAR(256) OUTPUT
AS
BEGIN
    SET NOCOUNT ON
    
    DECLARE @v_old_sttus_cd CHAR(2)
    DECLARE @v_ntnl_amt DECIMAL(22,2)
    DECLARE @v_mtm_val DECIMAL(18,6)
    DECLARE @v_vers_num INT
    
    SELECT @p_err_code = 0, @p_err_msg = ''
    
    -- Get current trade details
    SELECT 
        @v_old_sttus_cd = TRD_STTUS_CD,
        @v_ntnl_amt = NTNL_AMT,
        @v_mtm_val = MTM_VAL,
        @v_vers_num = TRD_VERS_NUM
    FROM TRD_BLOTTER
    WHERE TRD_ID = @p_trd_id
    
    IF @@ROWCOUNT = 0
    BEGIN
        SELECT @p_err_code = 2001, @p_err_msg = 'Trade not found: ' + CAST(@p_trd_id AS VARCHAR)
        RETURN @p_err_code
    END
    
    -- Validate status transition
    -- Only allow: PE -> VF -> BK, or any -> CN/ER
    IF @v_old_sttus_cd = 'CN'
    BEGIN
        SELECT @p_err_code = 2002, @p_err_msg = 'Cannot modify cancelled trade'
        RETURN @p_err_code
    END
    
    IF @v_old_sttus_cd = 'BK' AND @p_new_sttus_cd NOT IN ('CN', 'ER')
    BEGIN
        SELECT @p_err_code = 2003, @p_err_msg = 'Booked trades can only be cancelled or marked as error'
        RETURN @p_err_code
    END
    
    BEGIN TRANSACTION
    
    BEGIN TRY
        -- Update trade status
        UPDATE TRD_BLOTTER
        SET TRD_STTUS_CD = @p_new_sttus_cd,
            TRD_VERS_NUM = TRD_VERS_NUM + 1,
            UPDTD_TS = GETDATE()
        WHERE TRD_ID = @p_trd_id
        
        -- Insert history (V4)
        -- #ifdef SCHEMA_V4_CURRENT
        INSERT INTO TRD_HIST (
            TRD_ID, TRD_VERS_NUM, TRD_STTUS_CD, NTNL_AMT, MTM_VAL,
            CHNG_TYPE_CD, CHNG_REASON_TX, CHNG_BY_USR
        ) VALUES (
            @p_trd_id, @v_vers_num + 1, @p_new_sttus_cd, @v_ntnl_amt, @v_mtm_val,
            'STTS', @p_chng_reason_tx, @p_chng_by_usr
        )
        -- #endif
        
        COMMIT TRANSACTION
        
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION
        SELECT @p_err_code = ERROR_NUMBER(), @p_err_msg = ERROR_MESSAGE()
        RETURN @p_err_code
    END CATCH
    
    RETURN 0
END
GO

-- ============================================================================
-- SP_GET_DAILY_TRADES
-- ============================================================================
-- Gets daily trade report
-- ============================================================================
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_GET_DAILY_TRADES' AND type = 'P')
    DROP PROCEDURE SP_GET_DAILY_TRADES
GO

CREATE PROCEDURE SP_GET_DAILY_TRADES
    @p_trd_dt           DATETIME = NULL,
    @p_book_nm          VARCHAR(16) = NULL,
    @p_cpty_id          INT = NULL
AS
BEGIN
    SET NOCOUNT ON
    
    IF @p_trd_dt IS NULL
        SELECT @p_trd_dt = CAST(GETDATE() AS DATE)
    
    SELECT 
        t.TRD_ID,
        t.TRD_REF_NM,
        c.CPTY_CD,
        c.CPTY_NM,
        p.PRD_TYP_CD,
        t.TRD_STTUS_CD,
        t.TRD_DT,
        t.NTNL_AMT,
        t.NTNL_CCY_CD,
        t.NTNL_AMT_USD,
        t.MTM_VAL,
        t.BOOK_NM,
        t.TRD_USR_ID
    FROM TRD_BLOTTER t
    INNER JOIN CPTY_MSTR c ON t.CPTY_ID = c.CPTY_ID
    INNER JOIN PRD_TYP_REF p ON t.PRD_TYP_ID = p.PRD_TYP_ID
    WHERE t.TRD_DT >= @p_trd_dt
      AND t.TRD_DT < DATEADD(DAY, 1, @p_trd_dt)
      AND (@p_book_nm IS NULL OR t.BOOK_NM = @p_book_nm)
      AND (@p_cpty_id IS NULL OR t.CPTY_ID = @p_cpty_id)
    ORDER BY t.TRD_ID
END
GO

-- ============================================================================
-- LEGACY PROCEDURES (DEPRECATED)
-- ============================================================================
-- These procedures are kept for backwards compatibility with V1 applications
-- Do not use in new code
-- ============================================================================

-- Legacy: Get counterparty by old code format
-- IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SP_GET_CPTY_LEGACY' AND type = 'P')
--     DROP PROCEDURE SP_GET_CPTY_LEGACY
-- GO
-- 
-- CREATE PROCEDURE SP_GET_CPTY_LEGACY
--     @p_legacy_pty_cd VARCHAR(16)
-- AS
-- BEGIN
--     -- This procedure is deprecated
--     -- Use CPTY_CD instead of LEGACY_PTY_CD
--     SELECT * FROM CPTY_MSTR WHERE CPTY_CD = @p_legacy_pty_cd
-- END
-- GO

-- $Log: sp_trade_validation.sql,v $
-- Revision 1.89  2019/06/15 14:20:00  apatel
-- Added portfolio filtering to daily trades
--
-- Revision 1.88  2018/03/22 11:00:00  apatel
-- Fixed status transition validation bug
--
-- Revision 1.87  2017/11/10 09:30:00  apatel
-- Added history tracking for V4
--
-- Revision 1.86  2016/09/01 10:00:00  apatel
-- Created SP_INSERT_TRADE_V2 with notional support
--
-- Revision 1.85  2007/05/22 14:00:00  mrodriguez
-- Added counterparty limit checking
--
-- Revision 1.84  2004/03/20 09:00:00  jthompson
-- Initial stored procedures for V1
-- ============================================================================
